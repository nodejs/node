// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/baseline/baseline-batch-compiler.h"

// TODO(v8:11421): Remove #if once baseline compiler is ported to other
// architectures.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 ||     \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_MIPS64 || \
    V8_TARGET_ARCH_MIPS

#include "src/baseline/baseline-compiler.h"
#include "src/codegen/compiler.h"
#include "src/execution/isolate.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-function-inl.h"

namespace v8 {
namespace internal {
namespace baseline {

BaselineBatchCompiler::BaselineBatchCompiler(Isolate* isolate)
    : isolate_(isolate),
      compilation_queue_(Handle<WeakFixedArray>::null()),
      last_index_(0),
      estimated_instruction_size_(0),
      enabled_(true) {}

BaselineBatchCompiler::~BaselineBatchCompiler() {
  if (!compilation_queue_.is_null()) {
    GlobalHandles::Destroy(compilation_queue_.location());
    compilation_queue_ = Handle<WeakFixedArray>::null();
  }
}

bool BaselineBatchCompiler::EnqueueFunction(Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
  // Early return if the function is compiled with baseline already or it is not
  // suitable for baseline compilation.
  if (shared->HasBaselineData()) return true;
  if (!CanCompileWithBaseline(isolate_, *shared)) return false;

  // Immediately compile the function if batch compilation is disabled.
  if (!is_enabled()) {
    IsCompiledScope is_compiled_scope(
        function->shared().is_compiled_scope(isolate_));
    return Compiler::CompileBaseline(
        isolate_, function, Compiler::CLEAR_EXCEPTION, &is_compiled_scope);
  }

  int estimated_size;
  {
    DisallowHeapAllocation no_gc;
    estimated_size = BaselineCompiler::EstimateInstructionSize(
        shared->GetBytecodeArray(isolate_));
  }
  estimated_instruction_size_ += estimated_size;
  if (FLAG_trace_baseline_batch_compilation) {
    CodeTracer::Scope trace_scope(isolate_->GetCodeTracer());
    PrintF(trace_scope.file(),
           "[Baseline batch compilation] Enqueued function ");
    function->PrintName(trace_scope.file());
    PrintF(trace_scope.file(),
           " with estimated size %d (current budget: %d/%d)\n", estimated_size,
           estimated_instruction_size_,
           FLAG_baseline_batch_compilation_threshold);
  }
  if (ShouldCompileBatch()) {
    if (FLAG_trace_baseline_batch_compilation) {
      CodeTracer::Scope trace_scope(isolate_->GetCodeTracer());
      PrintF(trace_scope.file(),
             "[Baseline batch compilation] Compiling current batch of %d "
             "functions\n",
             (last_index_ + 1));
    }
    CompileBatch(function);
    return true;
  }
  EnsureQueueCapacity();
  compilation_queue_->Set(last_index_++, HeapObjectReference::Weak(*shared));
  return false;
}

void BaselineBatchCompiler::EnsureQueueCapacity() {
  if (compilation_queue_.is_null()) {
    compilation_queue_ = isolate_->global_handles()->Create(
        *isolate_->factory()->NewWeakFixedArray(kInitialQueueSize,
                                                AllocationType::kOld));
    return;
  }
  if (last_index_ >= compilation_queue_->length()) {
    Handle<WeakFixedArray> new_queue =
        isolate_->factory()->CopyWeakFixedArrayAndGrow(compilation_queue_,
                                                       last_index_);
    GlobalHandles::Destroy(compilation_queue_.location());
    compilation_queue_ = isolate_->global_handles()->Create(*new_queue);
  }
}

void BaselineBatchCompiler::CompileBatch(Handle<JSFunction> function) {
  CodePageCollectionMemoryModificationScope batch_allocation(isolate_->heap());
  {
    IsCompiledScope is_compiled_scope(
        function->shared().is_compiled_scope(isolate_));
    Compiler::CompileBaseline(isolate_, function, Compiler::CLEAR_EXCEPTION,
                              &is_compiled_scope);
  }
  for (int i = 0; i < last_index_; i++) {
    MaybeObject maybe_sfi = compilation_queue_->Get(i);
    MaybeCompileFunction(maybe_sfi);
    compilation_queue_->Set(i, HeapObjectReference::ClearedValue(isolate_));
  }
  ClearBatch();
}

bool BaselineBatchCompiler::ShouldCompileBatch() const {
  return estimated_instruction_size_ >=
         FLAG_baseline_batch_compilation_threshold;
}

bool BaselineBatchCompiler::MaybeCompileFunction(MaybeObject maybe_sfi) {
  HeapObject heapobj;
  // Skip functions where the weak reference is no longer valid.
  if (!maybe_sfi.GetHeapObjectIfWeak(&heapobj)) return false;
  Handle<SharedFunctionInfo> shared =
      handle(SharedFunctionInfo::cast(heapobj), isolate_);
  // Skip functions where the bytecode has been flushed.
  if (!shared->is_compiled()) return false;

  IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate_));
  return Compiler::CompileSharedWithBaseline(
      isolate_, shared, Compiler::CLEAR_EXCEPTION, &is_compiled_scope);
}

void BaselineBatchCompiler::ClearBatch() {
  estimated_instruction_size_ = 0;
  last_index_ = 0;
}

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#else

namespace v8 {
namespace internal {
namespace baseline {

BaselineBatchCompiler::BaselineBatchCompiler(Isolate* isolate)
    : isolate_(isolate),
      compilation_queue_(Handle<WeakFixedArray>::null()),
      last_index_(0),
      estimated_instruction_size_(0),
      enabled_(false) {}

BaselineBatchCompiler::~BaselineBatchCompiler() {
  if (!compilation_queue_.is_null()) {
    GlobalHandles::Destroy(compilation_queue_.location());
    compilation_queue_ = Handle<WeakFixedArray>::null();
  }
}

}  // namespace baseline
}  // namespace internal
}  // namespace v8

#endif
