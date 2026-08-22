#include "lexer.hpp"

#include <cctype>

std::string identifier_string;
double number_value;

int get_token() {
  static int last_char = ' ';

  // Skip whitespace
  while (std::isspace(last_char)) {
    last_char = std::getchar();
  }

  // Identifier: [a-zA-Z][a-zA-Z0-9]*
  if (std::isalpha(last_char)) {
    identifier_string = static_cast<char>(last_char);
    while (std::isalnum(last_char = std::getchar())) {
      identifier_string += static_cast<char>(last_char);
    }

    if (identifier_string == "def") {
      return TOKEN_DEF;
    }
    if (identifier_string == "extern") {
      return TOKEN_EXTERN;
    }

    return TOKEN_IDENTIFIER;
  }

  // Number: [0-9]*\.?[0-9]+
  if (std::isdigit(last_char) || last_char == '.') {
    std::string number_str;
    do {
      number_str += static_cast<char>(last_char);
      last_char = std::getchar();
    } while (std::isdigit(last_char) || last_char == '.');

    number_value = std::strtod(number_str.c_str(), nullptr);

    return TOKEN_NUMBER;
  }

  if (last_char == '#') {
    // Comment until end of line
    do {
      last_char = std::getchar();
    } while (last_char != EOF && last_char != '\n' && last_char != '\r');

    // Skip comment and parse next token
    if (last_char != EOF) {
      return get_token();
    }
  }

  // End of input has been reached
  if (last_char == EOF) {
    return TOKEN_EOF;
  }

  // Otherwise, just return this character as its ASCII value
  const auto this_char = last_char;
  last_char = std::getchar();

  return this_char;
}
