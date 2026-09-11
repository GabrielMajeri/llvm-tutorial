#pragma once

#include <memory>
#include <string>
#include <vector>

#include <llvm/IR/Value.h>

/// @brief Base class for all expression nodes.
class Expression {
public:
  virtual ~Expression() = default;

  /// @brief Generates an LLVM Intermediate Representation (IR) value
  /// corresponding to this AST node.
  virtual llvm::Value *codegen() const = 0;
};

/// @brief Expression class for numeric literals.
class NumberExpression : public Expression {
  double value;

public:
  explicit NumberExpression(const double value) : value{value} {}

  llvm::Value *codegen() const override;
};

/// @brief Expression class for variable references.
class VariableExpression : public Expression {
  std::string name;

public:
  explicit VariableExpression(std::string name) : name{std::move(name)} {}

  llvm::Value *codegen() const override;
};

/// @brief Expression class for binary operations.
class BinaryExpression : public Expression {
  char operation;
  std::unique_ptr<Expression> lhs, rhs;

public:
  BinaryExpression(const char operation, std::unique_ptr<Expression> lhs,
                   std::unique_ptr<Expression> rhs)
      : operation{operation}, lhs{std::move(lhs)}, rhs{std::move(rhs)} {}

  llvm::Value *codegen() const override;
};

/// @brief Expression class for function calls.
class CallExpression : public Expression {
  std::string callee;
  std::vector<std::unique_ptr<Expression>> arguments;

public:
  CallExpression(std::string callee,
                 std::vector<std::unique_ptr<Expression>> arguments)
      : callee{std::move(callee)}, arguments{std::move(arguments)} {}

  llvm::Value *codegen() const override;
};

/// @brief Represents the prototype/signature for a function,
/// without recording its contents.
class FunctionPrototype {
  std::string name;
  std::vector<std::string> args;

public:
  FunctionPrototype(std::string name, std::vector<std::string> args)
      : name{std::move(name)}, args{std::move(args)} {}

  inline const std::string &get_name() const { return name; }

  llvm::Function *codegen() const;
};

/// @brief Represents a function definition itself.
class FunctionDefinition {
  std::unique_ptr<FunctionPrototype> prototype;
  std::unique_ptr<Expression> body;

public:
  FunctionDefinition(std::unique_ptr<FunctionPrototype> prototype,
                     std::unique_ptr<Expression> body)
      : prototype{std::move(prototype)}, body{std::move(body)} {}

  const FunctionPrototype &get_prototype() const { return *prototype; }

  llvm::Function *codegen() const;
};

/// @brief Initializes the LLVM global context, module and pass managers.
void initialize_llvm();

/// @brief Prints out all generated code from the current IR module.
void print_generated_code();

namespace llvm::orc {
class ThreadSafeModule;
}

/// @brief Creates a thread-safe ORC module from the current LLVM module and
/// returns it.
llvm::orc::ThreadSafeModule create_thread_safe_module();
