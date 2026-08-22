#include <cctype>

#include <string>
#include <variant>

enum class Token {
  // End-of-file
  TOK_EOF,

  // Commands
  TOK_DEF,
  TOK_EXTERN,

  // Primary tokens
  TOK_IDENTIFIER,
  TOK_NUMBER,
};

static std::string identifier_string;
static double number_value;

/// Return the next token from standard input.
static std::variant<Token, int> get_token() {
  static int last_char = ' ';

  // Skip whitespace
  while (std::isspace(last_char)) {
    last_char = std::getchar();
  }

  // Identifier: [a-zA-Z][a-zA-Z0-9]*
  if (std::isalpha(last_char)) {
    identifier_string = last_char;
    while (std::isalnum((last_char = std::getchar()))) {
      identifier_string += last_char;
    }

    if (identifier_string == "def") {
      return Token::TOK_DEF;
    }
    if (identifier_string == "extern") {
      return Token::TOK_EXTERN;
    }

    return Token::TOK_IDENTIFIER;
  }

  // Number: [0-9]*\.?[0-9]+
  if (std::isdigit(last_char) || last_char == '.') {
    std::string number_str;
    do {
      number_str += last_char;
      last_char = std::getchar();
    } while (std::isdigit(last_char) || last_char == '.');

    number_value = std::strtod(number_str.c_str(), nullptr);

    return Token::TOK_NUMBER;
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

  if (last_char == EOF) {
    return Token::TOK_EOF;
  }

  // Otherwise, just return this character as its ASCII value
  int this_char = last_char;

  last_char = std::getchar();

  return this_char;
}
