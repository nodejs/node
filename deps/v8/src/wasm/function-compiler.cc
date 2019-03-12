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

const char* GetExecutionTierAsString(ExecutionTier tier) {
  switch (tier) {
    case ExecutionTier::kBaseline:
      return "liftoff";
    case ExecutionTier::kOptimized:
      return "turbofan";
    case ExecutionTier::kInterpreter:
      return "interpreter";
  }
  UNREACHABLE();
}

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

  // While the buffer is grown, we need to temporarily also keep the old
  // buffer alive.
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
  return FLAG_liftoff && module->origin == kWasmOrigin
             ? ExecutionTier::kBaseline
             : ExecutionTier::kOptimized;
}

WasmCompilationUnit::WasmCompilationUnit(WasmEngine* wasm_engine, int index,
                                         ExecutionTier tier)
    : wasm_engine_(wasm_engine), func_index_(index), tier_(tier) {
  if (V8_UNLIKELY(FLAG_wasm_tier_mask_for_testing) && index < 32 &&
      (FLAG_wasm_tier_mask_for_testing & (1 << index))) {
    tier = ExecutionTier::kOptimized;
  }
  SwitchTier(tier);
}

// Declared here such that {LiftoffCompilationUnit} and
// {TurbofanWasmCompilationUnit} can be opaque in the header file.
WasmCompilationUnit::~WasmCompilationUnit() = default;

WasmCompilationResult WasmCompilationUnit::ExecuteCompilation(
    CompilationEnv* env,
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

  if (FLAG_trace_wasm_compiler) {
    PrintF("Compiling wasm function %d with %s\n\n", func_index_,
           GetExecutionTierAsString(tier_));
  }

  WasmCompilationResult result;
  switch (tier_) {
    case ExecutionTier::kBaseline:
      result =
          liftoff_unit_->ExecuteCompilation(env, func_body, counters, detected);
      if (result.succeeded()) break;
      // Otherwise, fall back to turbofan.
      SwitchTier(ExecutionTier::kOptimized);
      // TODO(wasm): We could actually stop or remove the tiering unit for this
      // function to avoid compiling it twice with TurboFan.
      V8_FALLTHROUGH;
    case ExecutionTier::kOptimized:
      result = turbofan_unit_->ExecuteCompilation(env, func_body, counters,
                                                  detected);
      break;
    case ExecutionTier::kInterpreter:
      UNREACHABLE();  // TODO(titzer): compile interpreter entry stub.
  }

  if (result.succeeded()) {
    counters->wasm_generated_code_size()->Increment(
        result.code_desc.instr_size);
    counters->wasm_reloc_size()->Increment(result.code_desc.reloc_size);
  }

  return result;
}

WasmCode* WasmCompilationUnit::Publish(WasmCompilationResult result,
                                       NativeModule* native_module) {
  if (!result.succeeded()) {
    native_module->compilation_state()->SetError(func_index_,
                                                 std::move(result.error));
    return nullptr;
  }

  // The {tier} argument specifies the requested tier, which can differ from the
  // actually executed tier stored in {unit->tier()}.
  DCHECK(result.succeeded());
  WasmCode::Tier code_tier = tier_ == ExecutionTier::kBaseline
                                 ? WasmCode::kLiftoff
                                 : WasmCode::kTurbofan;
  DCHECK_EQ(result.code_desc.buffer, result.instr_buffer.get());
  WasmCode* code = native_module->AddCode(
      func_index_, result.code_desc, result.frame_slot_count,
      result.safepoint_table_offset, result.handler_table_offset,
      std::move(result.protected_instructions),
      std::move(result.source_positions), WasmCode::kFunction, code_tier);
  // TODO(clemensh): Merge this into {AddCode}?
  native_module->PublishCode(code);
  return code;
}

void WasmCompilationUnit::SwitchTier(ExecutionTier new_tier) {
  // This method is being called in the constructor, where neither
  // {liftoff_unit_} nor {turbofan_unit_} are set, or to switch tier from
  // kLiftoff to kTurbofan, in which case {liftoff_unit_} is already set.
  tier_ = new_tier;
  switch (new_tier) {
    case ExecutionTier::kBaseline:
      DCHECK(!turbofan_unit_);
      DCHECK(!liftoff_unit_);
      liftoff_unit_.reset(new LiftoffCompilationUnit(this));
      return;
    case ExecutionTier::kOptimized:
      DCHECK(!turbofan_unit_);
      liftoff_unit_.reset();
      turbofan_unit_.reset(new compiler::TurbofanWasmCompilationUnit(this));
      return;
    case ExecutionTier::kInterpreter:
      UNREACHABLE();  // TODO(titzer): allow compiling interpreter entry stub.
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

  WasmCompilationUnit unit(isolate->wasm_engine(), function->func_index, tier);
  CompilationEnv env = native_module->CreateCompilationEnv();
  WasmCompilationResult result = unit.ExecuteCompilation(
      &env, native_module->compilation_state()->GetWireBytesStorage(),
      isolate->counters(), detected);
  unit.Publish(std::move(result), native_module);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
