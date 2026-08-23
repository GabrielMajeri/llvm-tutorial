#include <iostream>

#include <llvm/IR/Function.h>

#include "ast.hpp"
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
      std::cerr << "Parsed a top-level expression:\n";
      expression_ir->print(llvm::errs());
      std::cerr << std::endl;
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
