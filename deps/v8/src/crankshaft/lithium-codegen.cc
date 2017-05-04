// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/lithium-codegen.h"

#include <sstream>

#include "src/objects-inl.h"

#if V8_TARGET_ARCH_IA32
#include "src/crankshaft/ia32/lithium-ia32.h"  // NOLINT
#include "src/crankshaft/ia32/lithium-codegen-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/crankshaft/x64/lithium-x64.h"  // NOLINT
#include "src/crankshaft/x64/lithium-codegen-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/crankshaft/arm/lithium-arm.h"  // NOLINT
#include "src/crankshaft/arm/lithium-codegen-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/crankshaft/arm64/lithium-arm64.h"  // NOLINT
#include "src/crankshaft/arm64/lithium-codegen-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/crankshaft/mips/lithium-mips.h"  // NOLINT
#include "src/crankshaft/mips/lithium-codegen-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/crankshaft/mips64/lithium-mips64.h"  // NOLINT
#include "src/crankshaft/mips64/lithium-codegen-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/crankshaft/x87/lithium-x87.h"  // NOLINT
#include "src/crankshaft/x87/lithium-codegen-x87.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/crankshaft/ppc/lithium-ppc.h"          // NOLINT
#include "src/crankshaft/ppc/lithium-codegen-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/crankshaft/s390/lithium-s390.h"          // NOLINT
#include "src/crankshaft/s390/lithium-codegen-s390.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

#include "src/globals.h"

namespace v8 {
namespace internal {


HGraph* LCodeGenBase::graph() const {
  return chunk()->graph();
}

LCodeGenBase::LCodeGenBase(LChunk* chunk, MacroAssembler* assembler,
                           CompilationInfo* info)
    : chunk_(static_cast<LPlatformChunk*>(chunk)),
      masm_(assembler),
      info_(info),
      zone_(info->zone()),
      status_(UNUSED),
      current_block_(-1),
      current_instruction_(-1),
      instructions_(chunk->instructions()),
      deoptimizations_(4, info->zone()),
      deoptimization_literals_(8, info->zone()),
      translations_(info->zone()),
      inlined_function_count_(0),
      last_lazy_deopt_pc_(0),
      osr_pc_offset_(-1),
      source_position_table_builder_(info->zone(),
                                     info->SourcePositionRecordingMode()) {}

Isolate* LCodeGenBase::isolate() const { return info_->isolate(); }

bool LCodeGenBase::GenerateBody() {
  DCHECK(is_generating());
  bool emit_instructions = true;
  LCodeGen* codegen = static_cast<LCodeGen*>(this);
  for (current_instruction_ = 0;
       !is_aborted() && current_instruction_ < instructions_->length();
       current_instruction_++) {
    LInstruction* instr = instructions_->at(current_instruction_);

    // Don't emit code for basic blocks with a replacement.
    if (instr->IsLabel()) {
      emit_instructions = !LLabel::cast(instr)->HasReplacement() &&
          (!FLAG_unreachable_code_elimination ||
           instr->hydrogen_value()->block()->IsReachable());
      if (FLAG_code_comments && !emit_instructions) {
        Comment(
            ";;; <@%d,#%d> -------------------- B%d (unreachable/replaced) "
            "--------------------",
            current_instruction_,
            instr->hydrogen_value()->id(),
            instr->hydrogen_value()->block()->block_id());
      }
    }
    if (!emit_instructions) continue;

    if (FLAG_code_comments && instr->HasInterestingComment(codegen)) {
      Comment(";;; <@%d,#%d> %s",
              current_instruction_,
              instr->hydrogen_value()->id(),
              instr->Mnemonic());
    }

    GenerateBodyInstructionPre(instr);

    HValue* value = instr->hydrogen_value();
    if (value->position().IsKnown()) {
      RecordAndWritePosition(value->position());
    }

    instr->CompileToNative(codegen);

    GenerateBodyInstructionPost(instr);
  }
  EnsureSpaceForLazyDeopt(Deoptimizer::patch_size());
  last_lazy_deopt_pc_ = masm()->pc_offset();
  return !is_aborted();
}


void LCodeGenBase::CheckEnvironmentUsage() {
#ifdef DEBUG
  bool dead_block = false;
  for (int i = 0; i < instructions_->length(); i++) {
    LInstruction* instr = instructions_->at(i);
    HValue* hval = instr->hydrogen_value();
    if (instr->IsLabel()) dead_block = LLabel::cast(instr)->HasReplacement();
    if (dead_block || !hval->block()->IsReachable()) continue;

    HInstruction* hinstr = HInstruction::cast(hval);
    if (!hinstr->CanDeoptimize() && instr->HasEnvironment()) {
      V8_Fatal(__FILE__, __LINE__, "CanDeoptimize is wrong for %s (%s)",
               hinstr->Mnemonic(), instr->Mnemonic());
    }

    if (instr->HasEnvironment() && !instr->environment()->has_been_used()) {
      V8_Fatal(__FILE__, __LINE__, "unused environment for %s (%s)",
               hinstr->Mnemonic(), instr->Mnemonic());
    }
  }
#endif
}

void LCodeGenBase::RecordAndWritePosition(SourcePosition pos) {
  if (!pos.IsKnown()) return;
  source_position_table_builder_.AddPosition(masm_->pc_offset(), pos, false);
}

void LCodeGenBase::Comment(const char* format, ...) {
  if (!FLAG_code_comments) return;
  char buffer[4 * KB];
  StringBuilder builder(buffer, arraysize(buffer));
  va_list arguments;
  va_start(arguments, format);
  builder.AddFormattedList(format, arguments);
  va_end(arguments);

  // Copy the string before recording it in the assembler to avoid
  // issues when the stack allocated buffer goes out of scope.
  size_t length = builder.position();
  Vector<char> copy = Vector<char>::New(static_cast<int>(length) + 1);
  MemCopy(copy.start(), builder.Finalize(), copy.length());
  masm()->RecordComment(copy.start());
}


void LCodeGenBase::DeoptComment(const Deoptimizer::DeoptInfo& deopt_info) {
  SourcePosition position = deopt_info.position;
  int deopt_id = deopt_info.deopt_id;
  if (masm()->isolate()->NeedsSourcePositionsForProfiling()) {
    masm()->RecordDeoptReason(deopt_info.deopt_reason, position, deopt_id);
  }
}


int LCodeGenBase::GetNextEmittedBlock() const {
  for (int i = current_block_ + 1; i < graph()->blocks()->length(); ++i) {
    if (!graph()->blocks()->at(i)->IsReachable()) continue;
    if (!chunk_->GetLabel(i)->HasReplacement()) return i;
  }
  return -1;
}


void LCodeGenBase::Abort(BailoutReason reason) {
  info()->AbortOptimization(reason);
  status_ = ABORTED;
}


void LCodeGenBase::Retry(BailoutReason reason) {
  info()->RetryOptimization(reason);
  status_ = ABORTED;
}


void LCodeGenBase::AddDeprecationDependency(Handle<Map> map) {
  if (map->is_deprecated()) return Retry(kMapBecameDeprecated);
  chunk_->AddDeprecationDependency(map);
}


void LCodeGenBase::AddStabilityDependency(Handle<Map> map) {
  if (!map->is_stable()) return Retry(kMapBecameUnstable);
  chunk_->AddStabilityDependency(map);
}


int LCodeGenBase::DefineDeoptimizationLiteral(Handle<Object> literal) {
  int result = deoptimization_literals_.length();
  for (int i = 0; i < deoptimization_literals_.length(); ++i) {
    if (deoptimization_literals_[i].is_identical_to(literal)) return i;
  }
  deoptimization_literals_.Add(literal, zone());
  return result;
}


void LCodeGenBase::WriteTranslationFrame(LEnvironment* environment,
                                         Translation* translation) {
  int translation_size = environment->translation_size();
  // The output frame height does not include the parameters.
  int height = translation_size - environment->parameter_count();

  switch (environment->frame_type()) {
    case JS_FUNCTION: {
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginJSFrame(environment->ast_id(), shared_id, height);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case JS_CONSTRUCT: {
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginConstructStubFrame(BailoutId::ConstructStubInvoke(),
                                           shared_id, translation_size);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case JS_GETTER: {
      DCHECK_EQ(1, translation_size);
      DCHECK_EQ(0, height);
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginGetterStubFrame(shared_id);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case JS_SETTER: {
      DCHECK_EQ(2, translation_size);
      DCHECK_EQ(0, height);
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginSetterStubFrame(shared_id);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case TAIL_CALLER_FUNCTION: {
      DCHECK_EQ(0, translation_size);
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginTailCallerFrame(shared_id);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case ARGUMENTS_ADAPTOR: {
      int shared_id = DefineDeoptimizationLiteral(
          environment->entry() ? environment->entry()->shared()
                               : info()->shared_info());
      translation->BeginArgumentsAdaptorFrame(shared_id, translation_size);
      if (info()->closure().is_identical_to(environment->closure())) {
        translation->StoreJSFrameFunction();
      } else {
        int closure_id = DefineDeoptimizationLiteral(environment->closure());
        translation->StoreLiteral(closure_id);
      }
      break;
    }
    case STUB:
      translation->BeginCompiledStubFrame(translation_size);
      break;
  }
}

namespace {

Handle<PodArray<InliningPosition>> CreateInliningPositions(
    CompilationInfo* info) {
  const CompilationInfo::InlinedFunctionList& inlined_functions =
      info->inlined_functions();
  if (inlined_functions.size() == 0) {
    return Handle<PodArray<InliningPosition>>::cast(
        info->isolate()->factory()->empty_byte_array());
  }
  Handle<PodArray<InliningPosition>> inl_positions =
      PodArray<InliningPosition>::New(
          info->isolate(), static_cast<int>(inlined_functions.size()), TENURED);
  for (size_t i = 0; i < inlined_functions.size(); ++i) {
    inl_positions->set(static_cast<int>(i), inlined_functions[i].position);
  }
  return inl_positions;
}

}  // namespace

void LCodeGenBase::PopulateDeoptimizationData(Handle<Code> code) {
  int length = deoptimizations_.length();
  if (length == 0) return;
  Handle<DeoptimizationInputData> data =
      DeoptimizationInputData::New(isolate(), length, TENURED);

  Handle<ByteArray> translations =
      translations_.CreateByteArray(isolate()->factory());
  data->SetTranslationByteArray(*translations);
  data->SetInlinedFunctionCount(Smi::FromInt(inlined_function_count_));
  data->SetOptimizationId(Smi::FromInt(info_->optimization_id()));
  if (info_->IsOptimizing()) {
    // Reference to shared function info does not change between phases.
    AllowDeferredHandleDereference allow_handle_dereference;
    data->SetSharedFunctionInfo(*info_->shared_info());
  } else {
    data->SetSharedFunctionInfo(Smi::kZero);
  }
  data->SetWeakCellCache(Smi::kZero);

  Handle<FixedArray> literals =
      factory()->NewFixedArray(deoptimization_literals_.length(), TENURED);
  {
    AllowDeferredHandleDereference copy_handles;
    for (int i = 0; i < deoptimization_literals_.length(); i++) {
      literals->set(i, *deoptimization_literals_[i]);
    }
    data->SetLiteralArray(*literals);
  }

  Handle<PodArray<InliningPosition>> inl_pos = CreateInliningPositions(info_);
  data->SetInliningPositions(*inl_pos);

  data->SetOsrAstId(Smi::FromInt(info_->osr_ast_id().ToInt()));
  data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));

  // Populate the deoptimization entries.
  for (int i = 0; i < length; i++) {
    LEnvironment* env = deoptimizations_[i];
    data->SetAstId(i, env->ast_id());
    data->SetTranslationIndex(i, Smi::FromInt(env->translation_index()));
    data->SetArgumentsStackHeight(i,
                                  Smi::FromInt(env->arguments_stack_height()));
    data->SetPc(i, Smi::FromInt(env->pc_offset()));
  }
  code->set_deoptimization_data(*data);
}


void LCodeGenBase::PopulateDeoptimizationLiteralsWithInlinedFunctions() {
  DCHECK_EQ(0, deoptimization_literals_.length());
  for (CompilationInfo::InlinedFunctionHolder& inlined :
       info()->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info()->shared_info())) {
      int index = DefineDeoptimizationLiteral(inlined.shared_info);
      inlined.RegisterInlinedFunctionId(index);
    }
  }
  inlined_function_count_ = deoptimization_literals_.length();

  // Define deoptimization literals for all unoptimized code objects of inlined
  // functions. This ensures unoptimized code is kept alive by optimized code.
  for (const CompilationInfo::InlinedFunctionHolder& inlined :
       info()->inlined_functions()) {
    if (!inlined.shared_info.is_identical_to(info()->shared_info())) {
      DefineDeoptimizationLiteral(inlined.inlined_code_object_root);
    }
  }
}

Deoptimizer::DeoptInfo LCodeGenBase::MakeDeoptInfo(
    LInstruction* instr, DeoptimizeReason deopt_reason, int deopt_id) {
  Deoptimizer::DeoptInfo deopt_info(instr->hydrogen_value()->position(),
                                    deopt_reason, deopt_id);
  return deopt_info;
}

}  // namespace internal
}  // namespace v8
