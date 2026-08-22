#pragma once

#include <memory>

class Expression;
class FunctionPrototype;
class FunctionDefinition;

std::unique_ptr<Expression> parse_expression();

std::unique_ptr<Expression> parse_identifier_expression();
std::unique_ptr<Expression> parse_primary();
std::unique_ptr<Expression> parse_paren_expression();
std::unique_ptr<Expression> parse_number_expression();

std::unique_ptr<Expression>
parse_binary_operation_rhs(int expression_precedence,
                           std::unique_ptr<Expression> lhs);

std::unique_ptr<FunctionPrototype> parse_prototype();
std::unique_ptr<FunctionDefinition> parse_definition();
std::unique_ptr<FunctionPrototype> parse_extern();

std::unique_ptr<FunctionDefinition> parse_top_level_expression();
