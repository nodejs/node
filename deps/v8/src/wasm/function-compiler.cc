// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/function-compiler.h"

#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/macro-assembler-inl.h"
#include "src/wasm/baseline/liftoff-compiler.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

class WasmInstructionBufferImpl {
 public:
  class View : public AssemblerBuffer {
   public:
    View(Vector<uint8_t> buffer, WasmInstructionBufferImpl* holder)
        : buffer_(buffer), holder_(holder) {}

    ~View() override {
      if (buffer_.start() == holder_->old_buffer_.start()) {
        DCHECK_EQ(buffer_.size(), holder_->old_buffer_.size());
        holder_->old_buffer_ = {};
      }
    }

    byte* start() const override { return buffer_.start(); }

    int size() const override { return static_cast<int>(buffer_.size()); }

    std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
      // If we grow, we must be the current buffer of {holder_}.
      DCHECK_EQ(buffer_.start(), holder_->buffer_.start());
      DCHECK_EQ(buffer_.size(), holder_->buffer_.size());
      DCHECK_NULL(holder_->old_buffer_);

      DCHECK_LT(size(), new_size);

      holder_->old_buffer_ = std::move(holder_->buffer_);
      holder_->buffer_ = OwnedVector<uint8_t>::New(new_size);
      return base::make_unique<View>(holder_->buffer_.as_vector(), holder_);
    }

   private:
    const Vector<uint8_t> buffer_;
    WasmInstructionBufferImpl* const holder_;
  };

  std::unique_ptr<AssemblerBuffer> CreateView() {
    DCHECK_NOT_NULL(buffer_);
    return base::make_unique<View>(buffer_.as_vector(), this);
  }

  std::unique_ptr<uint8_t[]> ReleaseBuffer() {
    DCHECK_NULL(old_buffer_);
    DCHECK_NOT_NULL(buffer_);
    return buffer_.ReleaseData();
  }

  bool released() const { return buffer_ == nullptr; }

 private:
  // The current buffer used to emit code.
  OwnedVector<uint8_t> buffer_ =
      OwnedVector<uint8_t>::New(AssemblerBase::kMinimalBufferSize);

  // While the buffer is grown, we need to temporarily also keep the old buffer
  // alive.
  OwnedVector<uint8_t> old_buffer_;
};

WasmInstructionBufferImpl* Impl(WasmInstructionBuffer* buf) {
  return reinterpret_cast<WasmInstructionBufferImpl*>(buf);
}

}  // namespace

// PIMPL interface WasmInstructionBuffer for WasmInstBufferImpl
WasmInstructionBuffer::~WasmInstructionBuffer() {
  Impl(this)->~WasmInstructionBufferImpl();
}

std::unique_ptr<AssemblerBuffer> WasmInstructionBuffer::CreateView() {
  return Impl(this)->CreateView();
}

std::unique_ptr<uint8_t[]> WasmInstructionBuffer::ReleaseBuffer() {
  return Impl(this)->ReleaseBuffer();
}

// static
std::unique_ptr<WasmInstructionBuffer> WasmInstructionBuffer::New() {
  return std::unique_ptr<WasmInstructionBuffer>{
      reinterpret_cast<WasmInstructionBuffer*>(
          new WasmInstructionBufferImpl())};
}
// End of PIMPL interface WasmInstructionBuffer for WasmInstBufferImpl

// static
ExecutionTier WasmCompilationUnit::GetDefaultExecutionTier(
    const WasmModule* module) {
  // Liftoff does not support the special asm.js opcodes, thus always compile
  // asm.js modules with TurboFan.
  if (module->origin == kAsmJsOrigin) return ExecutionTier::kTurbofan;
  if (FLAG_wasm_interpret_all) return ExecutionTier::kInterpreter;
  return FLAG_liftoff ? ExecutionTier::kLiftoff : ExecutionTier::kTurbofan;
}

WasmCompilationUnit::WasmCompilationUnit(int index, ExecutionTier tier)
    : func_index_(index), tier_(tier) {
  if (V8_UNLIKELY(FLAG_wasm_tier_mask_for_testing) && index < 32 &&
      (FLAG_wasm_tier_mask_for_testing & (1 << index))) {
    tier = ExecutionTier::kTurbofan;
  }
  SwitchTier(tier);
}

// Declared here such that {LiftoffCompilationUnit} and
// {TurbofanWasmCompilationUnit} can be opaque in the header file.
WasmCompilationUnit::~WasmCompilationUnit() = default;

WasmCompilationResult WasmCompilationUnit::ExecuteCompilation(
    WasmEngine* wasm_engine, CompilationEnv* env,
    const std::shared_ptr<WireBytesStorage>& wire_bytes_storage,
    Counters* counters, WasmFeatures* detected) {
  auto* func = &env->module->functions[func_index_];
  Vector<const uint8_t> code = wire_bytes_storage->GetCode(func->code);
  wasm::FunctionBody func_body{func->sig, func->code.offset(), code.start(),
                               code.end()};

  auto size_histogram = SELECT_WASM_COUNTER(counters, env->module->origin, wasm,
                                            function_size_bytes);
  size_histogram->AddSample(static_cast<int>(func_body.end - func_body.start));
  auto timed_histogram = SELECT_WASM_COUNTER(counters, env->module->origin,
                                             wasm_compile, function_time);
  TimedHistogramScope wasm_compile_function_time_scope(timed_histogram);

  // Exactly one compiler-specific unit must be set.
  DCHECK_EQ(1, !!liftoff_unit_ + !!turbofan_unit_ + !!interpreter_unit_);

  if (FLAG_trace_wasm_compiler) {
    const char* tier =
        liftoff_unit_ ? "liftoff" : turbofan_unit_ ? "turbofan" : "interpreter";
    PrintF("Compiling wasm function %d with %s\n\n", func_index_, tier);
  }

  WasmCompilationResult result;
  if (liftoff_unit_) {
    result = liftoff_unit_->ExecuteCompilation(wasm_engine->allocator(), env,
                                               func_body, counters, detected);
    if (!result.succeeded()) {
      // If Liftoff failed, fall back to turbofan.
      // TODO(wasm): We could actually stop or remove the tiering unit for this
      // function to avoid compiling it twice with TurboFan.
      SwitchTier(ExecutionTier::kTurbofan);
      DCHECK_NOT_NULL(turbofan_unit_);
    }
  }
  if (turbofan_unit_) {
    result = turbofan_unit_->ExecuteCompilation(wasm_engine, env, func_body,
                                                counters, detected);
  }
  if (interpreter_unit_) {
    result = interpreter_unit_->ExecuteCompilation(wasm_engine, env, func_body,
                                                   counters, detected);
  }
  result.func_index = func_index_;
  result.requested_tier = tier_;

  if (result.succeeded()) {
    counters->wasm_generated_code_size()->Increment(
        result.code_desc.instr_size);
    counters->wasm_reloc_size()->Increment(result.code_desc.reloc_size);
  }

  return result;
}

void WasmCompilationUnit::SwitchTier(ExecutionTier new_tier) {
  // This method is being called in the constructor, where neither
  // {liftoff_unit_} nor {turbofan_unit_} nor {interpreter_unit_} are set, or to
  // switch tier from kLiftoff to kTurbofan, in which case {liftoff_unit_} is
  // already set.
  switch (new_tier) {
    case ExecutionTier::kLiftoff:
      DCHECK(!turbofan_unit_);
      DCHECK(!liftoff_unit_);
      DCHECK(!interpreter_unit_);
      liftoff_unit_.reset(new LiftoffCompilationUnit());
      return;
    case ExecutionTier::kTurbofan:
      DCHECK(!turbofan_unit_);
      DCHECK(!interpreter_unit_);
      liftoff_unit_.reset();
      turbofan_unit_.reset(new compiler::TurbofanWasmCompilationUnit(this));
      return;
    case ExecutionTier::kInterpreter:
      DCHECK(!turbofan_unit_);
      DCHECK(!liftoff_unit_);
      DCHECK(!interpreter_unit_);
      interpreter_unit_.reset(new compiler::InterpreterCompilationUnit(this));
      return;
    case ExecutionTier::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

// static
void WasmCompilationUnit::CompileWasmFunction(Isolate* isolate,
                                              NativeModule* native_module,
                                              WasmFeatures* detected,
                                              const WasmFunction* function,
                                              ExecutionTier tier) {
  ModuleWireBytes wire_bytes(native_module->wire_bytes());
  FunctionBody function_body{function->sig, function->code.offset(),
                             wire_bytes.start() + function->code.offset(),
                             wire_bytes.start() + function->code.end_offset()};

  WasmCompilationUnit unit(function->func_index, tier);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = unit.ExecuteCompilation(
      isolate->wasm_engine(), &env,
      native_module->compilation_state()->GetWireBytesStorage(),
      isolate->counters(), detected);
  if (result.succeeded()) {
    WasmCodeRefScope code_ref_scope;
    native_module->AddCompiledCode(std::move(result));
  } else {
    native_module->compilation_state()->SetError();
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
