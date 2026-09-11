#pragma once

#include <memory>

#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/Mangling.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/DataLayout.h>

// Taken from
// https://github.com/llvm/llvm-project/blob/main/llvm/examples/Kaleidoscope/include/KaleidoscopeJIT.h
class KaleidoscopeJIT {
  std::unique_ptr<llvm::orc::ExecutionSession> execution_session;

  llvm::DataLayout data_layout;
  llvm::orc::MangleAndInterner mangler;

  llvm::orc::RTDyldObjectLinkingLayer object_layer;
  llvm::orc::IRCompileLayer compile_layer;

  llvm::orc::JITDylib &main_jit_dylib;

public:
  KaleidoscopeJIT(
      std::unique_ptr<llvm::orc::ExecutionSession> execution_session,
      llvm::orc::JITTargetMachineBuilder jtm_builder,
      llvm::DataLayout data_layout);

  ~KaleidoscopeJIT();

  static llvm::Expected<std::unique_ptr<KaleidoscopeJIT>> create();

  const llvm::DataLayout &get_data_layout() const { return data_layout; }

  llvm::orc::JITDylib &get_main_jit_dylib() { return main_jit_dylib; }

  llvm::Error add_module(llvm::orc::ThreadSafeModule module,
                         llvm::orc::ResourceTrackerSP tracker = nullptr);

  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef name);
};

KaleidoscopeJIT *get_jit();
