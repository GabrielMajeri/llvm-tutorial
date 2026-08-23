#include "ast.hpp"

#include <iostream>
#include <memory>
#include <unordered_map>

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>

using namespace llvm;

static std::unique_ptr<LLVMContext> llvm_context;
static std::unique_ptr<IRBuilder<>> ir_builder;
static std::unique_ptr<Module> llvm_module;
static std::unordered_map<std::string, Value *> named_values;

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
        left_value, Type::getDoubleTy(*llvm_context), "booltmp");
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

    // Validate the generated code
    verifyFunction(*function, &outs());

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

  // Create a new IR builder for our module
  ir_builder = std::make_unique<IRBuilder<>>(*llvm_context);
}

void print_generated_code() { llvm_module->print(outs(), nullptr); }
