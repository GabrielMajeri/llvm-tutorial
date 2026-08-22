#include "parser.hpp"

#include <iostream>
#include <unordered_map>

#include "ast.hpp"
#include "lexer.hpp"

// Provides a simple token buffer for the parser.
// The global static variable `current_token` is
// the token the parser is currently looking at.
static int current_token;

// Calling `get_next_token` reads another token from the lexer
// and updates the global variable with the result.
static int get_next_token() { return current_token = get_token(); }

// number_expr ::= number
std::unique_ptr<Expression> parse_number_expression() {
  // Read the number's value from the global variable
  auto result = std::make_unique<NumberExpression>(number_value);

  // Consume the number
  get_next_token();

  return result;
}

// paren_expr ::= '(' expression ')'
std::unique_ptr<Expression> parse_paren_expression() {
  // Eat a '('
  get_next_token();

  auto inner = parse_expression();
  if (!inner) {
    return nullptr;
  }

  if (current_token != ')') {
    std::cerr << "Error: expected ')'" << std::endl;
  }

  // Eat a ')'
  get_next_token();

  return inner;
}

// identifier_expr
//   ::= identifier
//   ::= identifier '(' expression* ')'
std::unique_ptr<Expression> parse_identifier_expression() {
  auto identifier_name = identifier_string;

  // Eat identifier (or parenthesis)
  get_next_token();

  // Simple variable reference
  if (current_token != '(') {
    return std::make_unique<VariableExpression>(std::move(identifier_name));
  }

  // Function call

  // Eat the '('
  get_next_token();

  std::vector<std::unique_ptr<Expression>> arguments;
  if (current_token != ')') {
    while (true) {
      if (auto argument = parse_expression()) {
        arguments.push_back(std::move(argument));
      } else {
        return nullptr;
      }

      if (current_token == ')') {
        break;
      }

      if (current_token != ',') {
        std::cerr << "Error: expected ')' or ',' in argument list";
        return nullptr;
      }

      get_next_token();
    }
  }

  // Eat the ')'
  get_next_token();

  return std::make_unique<CallExpression>(std::move(identifier_name),
                                          std::move(arguments));
}

// primary
//   ::= identifier_expr
//   ::= number_expr
//   ::= paren_expr
std::unique_ptr<Expression> parse_primary() {
  switch (current_token) {
  case '(':
    return parse_paren_expression();
  case TOKEN_IDENTIFIER:
    return parse_identifier_expression();
  case TOKEN_NUMBER:
    return parse_number_expression();
  default:
    std::cerr << "Error: unknown token when expecting an expression";
    return nullptr;
  }
}

// Holds the precedence for each binary operation that is defined.
// Lower numeric values mean those operators get evaluated later.
static std::unordered_map<char, int> binary_operation_precedence{
    {'<', 10}, {'+', 20}, {'-', 20}, {'*', 30} // TODO: add support for division
};

// Determine the precedence of the current token, if it is a binary operator.
static int get_token_precedence() {
  if (current_token < 0 or current_token > 255) {
    return -1;
  }

  const auto iter =
      binary_operation_precedence.find(static_cast<char>(current_token));
  if (iter == binary_operation_precedence.end()) {
    return -1;
  }

  return iter->second;
}

// expression
//   ::= primary binary_op_rhs
std::unique_ptr<Expression> parse_expression() {
  auto lhs = parse_primary();
  if (!lhs) {
    return nullptr;
  }

  return parse_binary_operation_rhs(0, std::move(lhs));
}

// binary_op_rhs
//   ::= ('+' primary)*
std::unique_ptr<Expression>
parse_binary_operation_rhs(const int expression_precedence,
                           std::unique_ptr<Expression> lhs) {
  // While there are still more terms/operators of the same precedence left
  while (true) {
    // If this is a binary operation, find its precedence
    const auto token_precedence = get_token_precedence();

    // If this is a binary operation that binds at least as tightly as the
    // current one, consume it, otherwise we are done.
    if (token_precedence < expression_precedence) {
      return lhs;
    }

    // We know this is a binary operation,
    // since it has a non-negative precedence
    int binary_op = current_token;

    // Eat the operator
    get_next_token();

    // Parse the primary expression left after it
    auto rhs = parse_primary();
    if (!rhs) {
      return nullptr;
    }

    // Look ahead at the next token and see if it is an operator of higher
    // precedence.
    if (const int next_token_precedence = get_token_precedence();
        token_precedence < next_token_precedence) {
      // Recursively call this same function to parse the next operator
      rhs = parse_binary_operation_rhs(token_precedence + 1, std::move(rhs));
      if (!rhs) {
        return nullptr;
      }
    }

    // Merge lhs and rhs
    lhs = std::make_unique<BinaryExpression>(binary_op, std::move(lhs),
                                             std::move(rhs));
  }
}

std::unique_ptr<FunctionPrototype> parse_prototype() {
  if (current_token != TOKEN_IDENTIFIER) {
    std::cerr << "Error: expected function name in prototype" << std::endl;
    return nullptr;
  }

  std::string function_name = identifier_string;
  get_next_token();

  if (current_token != '(') {
    std::cerr
        << "Error: expected '(' in function prototype, after function name"
        << std::endl;
    return nullptr;
  }

  std::vector<std::string> argument_names;

  while (true) {
    // No comma between function argument names, just whitespace.
    get_next_token();
    if (current_token == TOKEN_IDENTIFIER) {
      argument_names.push_back(identifier_string);
    } else {
      break;
    }
  }

  if (current_token != ')') {
    std::cerr << "Error: expected ')' to end function prototype" << std::endl;
    return nullptr;
  }

  // Eat the final ')'
  get_next_token();

  return std::make_unique<FunctionPrototype>(std::move(function_name),
                                             std::move(argument_names));
}

// definition ::= 'def' function_prototype expression
std::unique_ptr<FunctionDefinition> parse_definition() {
  // Eat the 'def' token
  get_next_token();

  auto proto = parse_prototype();
  if (!proto) {
    return nullptr;
  }

  if (auto expr = parse_expression()) {
    return std::make_unique<FunctionDefinition>(std::move(proto),
                                                std::move(expr));
  }

  return nullptr;
}

// external ::= 'extern' prototype
std::unique_ptr<FunctionPrototype> parse_extern() {
  // Eat the 'extern' token
  get_next_token();

  return parse_prototype();
}

std::unique_ptr<FunctionDefinition> parse_top_level_expression() {
  if (auto expr = parse_expression()) {
    // Make an anonymous prototype, with 0 arguments
    auto proto = std::make_unique<FunctionPrototype>(
        "__anon_expr", std::vector<std::string>());

    return std::make_unique<FunctionDefinition>(std::move(proto),
                                                std::move(expr));
  }

  return nullptr;
}

static void handle_definition() {
  if (parse_definition()) {
    std::cerr << "Parsed a definition" << std::endl;
  } else {
    // Skip token for error recovery
    get_next_token();
  }
}

static void handle_extern() {
  if (parse_extern()) {
    std::cerr << "Parsed an extern" << std::endl;
  } else {
    // Skip token for error recovery
    get_next_token();
  }
}

static void handle_top_level_expression() {
  // Evaluate a top-level expression into an anonymous function
  if (parse_top_level_expression()) {
    std::cerr << "Parsed a top-level expression" << std::endl;
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

  // Read-Eval-Print loop (REPL)
  main_loop();
}
