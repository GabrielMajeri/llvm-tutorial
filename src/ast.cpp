#include "ast.hpp"

#include <iostream>
#include <memory>
#include <unordered_map>

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include "jit.hpp"

using namespace llvm;

static std::unique_ptr<LLVMContext> llvm_context;
static std::unique_ptr<IRBuilder<>> ir_builder;
static std::unique_ptr<Module> llvm_module;
static std::unordered_map<std::string, Value *> named_values;

static std::unique_ptr<FunctionPassManager> function_pass_manager;
static std::unique_ptr<LoopAnalysisManager> loop_analysis_manager;
static std::unique_ptr<FunctionAnalysisManager> function_analysis_manager;
static std::unique_ptr<CGSCCAnalysisManager> cgscc_analysis_manager;
static std::unique_ptr<ModuleAnalysisManager> module_analysis_manager;
static std::unique_ptr<PassInstrumentationCallbacks>
    pass_instrumentation_callbacks;
static std::unique_ptr<StandardInstrumentations> standard_instrumentations;

Value *NumberExpression::codegen() const {
  return ConstantFP::get(*llvm_context, APFloat(value));
}

Value *VariableExpression::codegen() const {
  auto variable_value = named_values[name];
  if (!variable_value) {
    std::cerr << "Unknown variable name '" << name << "'" << std::endl;
  }
  return variable_value;
}

Value *BinaryExpression::codegen() const {
  const auto left_value = lhs->codegen();
  const auto right_value = rhs->codegen();
  if (!left_value || !right_value) {
    return nullptr;
  }

  switch (operation) {
  case '+':
    return ir_builder->CreateFAdd(left_value, right_value, "addtmp");

  case '-':
    return ir_builder->CreateFSub(left_value, right_value, "subtmp");

  case '*':
    return ir_builder->CreateFMul(left_value, right_value, "multmp");

  case '<': {
    const auto compare_result =
        ir_builder->CreateFCmpULT(left_value, right_value, "cmptmp");

    // Convert boolean comparison result (true or false) to double (1 or 0)
    return ir_builder->CreateUIToFP(
        compare_result, Type::getDoubleTy(*llvm_context), "booltmp");
  }

  default:
    std::cerr << "Error: invalid binary operator '" << operation << "'"
              << std::endl;
    return nullptr;
  }
}

Value *CallExpression::codegen() const {
  // Look up the name of the function in the module's table
  auto callee_fn = llvm_module->getFunction(callee);
  if (!callee_fn) {
    std::cerr << "Error: trying to call undefined function '" << callee << "'"
              << std::endl;
    return nullptr;
  }

  // Check that the number of argument matches
  const auto num_arguments = callee_fn->arg_size();
  if (num_arguments != arguments.size()) {
    std::cerr
        << "Error: incorrect number of arguments passed in call to function '"
        << callee << "'\n"
        << "(expected: " << callee_fn->arg_size()
        << ", received: " << arguments.size() << ")" << std::endl;
  }

  std::vector<Value *> effective_arguments;
  for (auto i = 0; i < num_arguments; ++i) {
    auto argument_value = arguments[i]->codegen();
    if (!argument_value) {
      return nullptr;
    }

    effective_arguments.push_back(argument_value);
  }

  return ir_builder->CreateCall(callee_fn, effective_arguments);
}

Function *FunctionPrototype::codegen() const {
  const auto double_type = Type::getDoubleTy(*llvm_context);

  // All our functions are of the type double(double, ..., double)
  const std::vector<Type *> argument_types(args.size(), double_type);

  const auto function_type =
      FunctionType::get(double_type, argument_types, false);

  const auto function = Function::Create(
      function_type, Function::ExternalLinkage, name, llvm_module.get());

  unsigned index = 0;
  for (auto &arg : function->args()) {
    arg.setName(args[index++]);
  }

  return function;
}

Function *FunctionDefinition::codegen() const {
  // First, check to see if the function exists from a previous 'extern'
  // declaration
  auto function = llvm_module->getFunction(prototype->get_name());

  if (!function) {
    function = prototype->codegen();
  }

  if (!function) {
    return nullptr;
  }

  if (!function->empty()) {
    std::cerr << "Error: function cannot be redefined" << std::endl;
    return nullptr;
  }

  // Create a new basic block for the function's instructions
  auto basic_block = BasicBlock::Create(*llvm_context, "entry", function);
  ir_builder->SetInsertPoint(basic_block);

  // Record the function arguments in the named_values dictionary
  named_values.clear();
  for (auto &argument : function->args()) {
    named_values[argument.getName().str()] = &argument;
  }

  if (auto *return_value = body->codegen()) {
    // The function's body is a single value; return it.
    ir_builder->CreateRet(return_value);

    // Validate the generated code, checking its consistency
    verifyFunction(*function, &outs());

    // Optimize the function
    function_pass_manager->run(*function, *function_analysis_manager);

    return function;
  }

  // Error reading body, remove the function from the module
  function->eraseFromParent();

  return nullptr;
}

void initialize_llvm() {
  // Create a new context
  llvm_context = std::make_unique<LLVMContext>();

  // Create a new module (the only one we'll need)
  llvm_module = std::make_unique<Module>("Kaleidoscope JIT", *llvm_context);
  llvm_module->setDataLayout(get_jit()->get_data_layout());

  // Create a new IR builder for our module
  ir_builder = std::make_unique<IRBuilder<>>(*llvm_context);

  // Create new pass and analysis managers
  function_pass_manager = std::make_unique<FunctionPassManager>();
  loop_analysis_manager = std::make_unique<LoopAnalysisManager>();
  function_analysis_manager = std::make_unique<FunctionAnalysisManager>();
  cgscc_analysis_manager = std::make_unique<CGSCCAnalysisManager>();
  module_analysis_manager = std::make_unique<ModuleAnalysisManager>();
  pass_instrumentation_callbacks =
      std::make_unique<PassInstrumentationCallbacks>();

  // Enable debug logging
  constexpr bool debug_logging = true;
  standard_instrumentations =
      std::make_unique<StandardInstrumentations>(*llvm_context,

                                                 debug_logging);

  standard_instrumentations->registerCallbacks(*pass_instrumentation_callbacks,
                                               module_analysis_manager.get());

  // Add transform passes
  // Simple "peephole" optimizations and bit-twiddling optimizations
  function_pass_manager->addPass(InstCombinePass());
  // Reassociate expressions
  function_pass_manager->addPass(ReassociatePass());
  // Identify and eliminate common subexpressions
  function_pass_manager->addPass(GVNPass());
  // Simplify the control-flow graph
  function_pass_manager->addPass(SimplifyCFGPass());

  // Register the analysis passes required for the transform passes
  PassBuilder pass_builder;
  pass_builder.registerModuleAnalyses(*module_analysis_manager);
  pass_builder.registerFunctionAnalyses((*function_analysis_manager));
  pass_builder.crossRegisterProxies(
      *loop_analysis_manager, *function_analysis_manager,
      *cgscc_analysis_manager, *module_analysis_manager);
}

void print_generated_code() { llvm_module->print(outs(), nullptr); }

llvm::orc::ThreadSafeModule create_thread_safe_module() {
  return {std::move(llvm_module), std::move(llvm_context)};
}
