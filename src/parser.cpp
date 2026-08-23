#include "parser.hpp"

#include <iostream>
#include <unordered_map>

#include "ast.hpp"
#include "lexer.hpp"

int current_token;

int get_next_token() { return current_token = get_token(); }

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
  static int counter = 0;
  if (auto expr = parse_expression()) {
    // Make an anonymous prototype, with 0 arguments
    auto proto = std::make_unique<FunctionPrototype>(
        "__anon_expr_" + std::to_string(counter++), std::vector<std::string>());

    return std::make_unique<FunctionDefinition>(std::move(proto),
                                                std::move(expr));
  }

  return nullptr;
}
