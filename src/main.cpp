#include <iostream>

#include <llvm/IR/Function.h>
#include <llvm/Support/TargetSelect.h>

#include "ast.hpp"
#include "jit.hpp"
#include "lexer.hpp"
#include "parser.hpp"

static void handle_definition() {
  if (auto function_ast_node = parse_definition()) {
    if (auto function_ir = function_ast_node->codegen()) {
      std::cerr << "Parsed a function definition:\n";
      function_ir->print(llvm::errs());
      std::cerr << std::endl;
    } else {
      std::cerr << "Function codegen failed!" << std::endl;
    }
  } else {
    // Skip token for error recovery
    get_next_token();
  }
}

static void handle_extern() {
  if (auto prototype_ast_node = parse_extern()) {
    if (auto prototype_ir = prototype_ast_node->codegen()) {
      std::cerr << "Parsed an extern function declaration:\n";
      prototype_ir->print(llvm::errs());
      std::cerr << std::endl;
    } else {
      std::cerr << "Extern declaration codegen failed!" << std::endl;
    }
  } else {
    // Skip token for error recovery
    get_next_token();
  }
}

static void handle_top_level_expression() {
  // Evaluate a top-level expression into an anonymous function
  if (auto expression_ast_node = parse_top_level_expression()) {
    if (auto expression_ir = expression_ast_node->codegen()) {
      // std::cerr << "Parsed a top-level expression:\n";
      // expression_ir->print(llvm::errs());
      // std::cerr << std::endl;

      // Create a ResourceTracker to track the JIT memory allocated to our
      // anonymous expression, to clean up later.
      auto resource_tracker =
          get_jit()->get_main_jit_dylib().createResourceTracker();

      auto thread_safe_module = create_thread_safe_module();
      if (auto error = get_jit()->add_module(std::move(thread_safe_module),
                                             resource_tracker)) {
        llvm::errs() << "Error while adding thread-safe module to JIT: "
                     << error << '\n';
        std::exit(1);
      }
      // Re-initialize module
      initialize_llvm();

      // Search the JIT for the __anon_expr symbol.
      auto expression_symbol_result =
          get_jit()->lookup(expression_ast_node->get_prototype().get_name());
      if (!expression_symbol_result) {
        llvm::errs()
            << "Error while looking up symbol for anonymous expression: "
            << expression_symbol_result.takeError() << '\n';
        std::exit(1);
      }

      auto expression_symbol = expression_symbol_result.get();

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      auto function_pointer =
          expression_symbol.getAddress().toPtr<double (*)()>();
      std::cerr << "Evaluated to " << function_pointer() << std::endl;

      // Delete the anonymous expression module from the JIT.
      if (auto error = resource_tracker->remove()) {
        std::exit(1);
      }

    } else {
      std::cerr << "Top-level expression codegen failed!" << std::endl;
    }
  } else {
    // Skip token for error recovery
    get_next_token();
  }
}

// top ::= definition | external | expression | ';'
static void main_loop() {
  while (true) {
    std::cout << "ready> ";
    switch (current_token) {
    case ';':
      // Ignore top-level semicolons
      get_next_token();
      break;
    case TOKEN_EOF:
      // End-of-file, exit loop
      return;
    case TOKEN_DEF:
      handle_definition();
      break;
    case TOKEN_EXTERN:
      handle_extern();
      break;
    default:
      // Treat as top-level expression
      handle_top_level_expression();
      break;
    }
  }
}

int main() {
  // Initialize support for the native (host) machine target
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  // Prime the first token.
  std::cout << "ready> ";
  get_next_token();

  // Initialize LLVM context and the main module
  initialize_llvm();

  // Read-Eval-Print loop (REPL)
  main_loop();

  // // Have LLVM print out all of the generated IR code
  // print_generated_code();
}
