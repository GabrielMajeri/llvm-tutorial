#pragma once

#include <string>

/// Token, as recognized by our language's tokenizer.
enum Token : int {
  // End-of-file
  TOKEN_EOF = -1,

  // Commands
  TOKEN_DEF = -2,
  TOKEN_EXTERN = -3,

  // Primary tokens
  TOKEN_IDENTIFIER = -4,
  TOKEN_NUMBER = -5,
};

extern std::string identifier_string;
extern double number_value;

/// Return the next token from standard input.
int get_token();
