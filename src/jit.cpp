#include "jit.hpp"

#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

using namespace llvm;
using namespace llvm::orc;

KaleidoscopeJIT::KaleidoscopeJIT(
    std::unique_ptr<ExecutionSession> execution_session,
    JITTargetMachineBuilder jtm_builder, DataLayout data_layout)
    : execution_session(std::move(execution_session)),
      data_layout(std::move(data_layout)),
      mangler(*this->execution_session, this->data_layout),
      object_layer(*this->execution_session,
                   []() { return std::make_unique<SectionMemoryManager>(); }),
      compile_layer(
          *this->execution_session, object_layer,
          std::make_unique<ConcurrentIRCompiler>(std::move(jtm_builder))),
      main_jit_dylib(this->execution_session->createBareJITDylib("<main>")) {

  // Load symbols from current process -- useful for easily adding new external
  // functions to the language
  main_jit_dylib.addGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
          data_layout.getGlobalPrefix())));

  // Special handling for COFF executable format on Windows
  if (jtm_builder.getTargetTriple().isOSBinFormatCOFF()) {
    object_layer.setOverrideObjectFlagsWithResponsibilityFlags(true);
    object_layer.setAutoClaimResponsibilityForObjectSymbols(true);
  }
}

KaleidoscopeJIT::~KaleidoscopeJIT() {
  if (auto error = execution_session->endSession()) {
    execution_session->reportError(std::move(error));
  }
}

Expected<std::unique_ptr<KaleidoscopeJIT>> KaleidoscopeJIT::create() {
  auto executor_process_control = SelfExecutorProcessControl::Create();
  if (!executor_process_control) {
    return executor_process_control.takeError();
  }

  auto execution_session =
      std::make_unique<ExecutionSession>(std::move(*executor_process_control));

  JITTargetMachineBuilder jtm_builder(
      execution_session->getExecutorProcessControl().getTargetTriple());

  auto data_layout = jtm_builder.getDefaultDataLayoutForTarget();
  if (!data_layout) {
    return data_layout.takeError();
  }

  return std::make_unique<KaleidoscopeJIT>(std::move(execution_session),
                                           std::move(jtm_builder),
                                           std::move(*data_layout));
}

Error KaleidoscopeJIT::add_module(ThreadSafeModule tsm,
                                  ResourceTrackerSP resource_tracker) {
  if (!resource_tracker) {
    resource_tracker = main_jit_dylib.getDefaultResourceTracker();
  }
  return compile_layer.add(resource_tracker, std::move(tsm));
}

Expected<ExecutorSymbolDef> KaleidoscopeJIT::lookup(StringRef name) {
  return execution_session->lookup({&main_jit_dylib}, mangler(name.str()));
}

KaleidoscopeJIT *get_jit() {
  static std::unique_ptr<KaleidoscopeJIT> jit;

  // Lazily initialize JIT on first use
  if (!jit) {
    auto result = KaleidoscopeJIT::create();
    if (!result) {
      errs() << "Failed to create Kaleidoscope JIT: " << result.takeError()
             << '\n';
      std::exit(1);
    }
    jit = std::move(result.get());
  }

  return jit.get();
}
