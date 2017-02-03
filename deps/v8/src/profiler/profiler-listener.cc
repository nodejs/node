// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/profiler-listener.h"

#include "src/deoptimizer.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {

ProfilerListener::ProfilerListener(Isolate* isolate)
    : function_and_resource_names_(isolate->heap()) {}

ProfilerListener::~ProfilerListener() {
  for (auto code_entry : code_entries_) {
    delete code_entry;
  }
}

void ProfilerListener::CallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry = NewCodeEntry(CodeEventListener::CALLBACK_TAG, GetName(name));
  rec->size = 1;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                       AbstractCode* code, const char* name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = NewCodeEntry(
      tag, GetFunctionName(name), CodeEntry::kEmptyNamePrefix,
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  RecordInliningInfo(rec->entry, code);
  rec->size = code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                       AbstractCode* code, Name* name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = NewCodeEntry(
      tag, GetFunctionName(name), CodeEntry::kEmptyNamePrefix,
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  RecordInliningInfo(rec->entry, code);
  rec->size = code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                       AbstractCode* code,
                                       SharedFunctionInfo* shared,
                                       Name* script_name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = NewCodeEntry(
      tag, GetFunctionName(shared->DebugName()), CodeEntry::kEmptyNamePrefix,
      GetName(InferScriptName(script_name, shared)),
      CpuProfileNode::kNoLineNumberInfo, CpuProfileNode::kNoColumnNumberInfo,
      NULL, code->instruction_start());
  RecordInliningInfo(rec->entry, code);
  rec->entry->FillFunctionInfo(shared);
  rec->size = code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                       AbstractCode* abstract_code,
                                       SharedFunctionInfo* shared,
                                       Name* script_name, int line,
                                       int column) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = abstract_code->address();
  Script* script = Script::cast(shared->script());
  JITLineInfoTable* line_table = NULL;
  if (script) {
    line_table = new JITLineInfoTable();
    int offset = abstract_code->IsCode() ? Code::kHeaderSize
                                         : BytecodeArray::kHeaderSize;
    int start_position = shared->start_position();
    int end_position = shared->end_position();
    for (SourcePositionTableIterator it(abstract_code->source_position_table());
         !it.done(); it.Advance()) {
      int position = it.source_position();
      // TODO(alph): in case of inlining the position may correspond to an
      // inlined function source code. Do not collect positions that fall
      // beyond the function source code. There's however a chance the
      // inlined function has similar positions but in another script. So
      // the proper fix is to store script_id in some form along with the
      // inlined function positions.
      if (position < start_position || position >= end_position) continue;
      int line_number = script->GetLineNumber(position) + 1;
      int pc_offset = it.code_offset() + offset;
      line_table->SetPosition(pc_offset, line_number);
    }
  }
  rec->entry = NewCodeEntry(
      tag, GetFunctionName(shared->DebugName()), CodeEntry::kEmptyNamePrefix,
      GetName(InferScriptName(script_name, shared)), line, column, line_table,
      abstract_code->instruction_start());
  RecordInliningInfo(rec->entry, abstract_code);
  RecordDeoptInlinedFrames(rec->entry, abstract_code);
  rec->entry->FillFunctionInfo(shared);
  rec->size = abstract_code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                       AbstractCode* code, int args_count) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = NewCodeEntry(
      tag, GetName(args_count), "args_count: ", CodeEntry::kEmptyResourceName,
      CpuProfileNode::kNoLineNumberInfo, CpuProfileNode::kNoColumnNumberInfo,
      NULL, code->instruction_start());
  RecordInliningInfo(rec->entry, code);
  rec->size = code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeMoveEvent(AbstractCode* from, Address to) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_MOVE);
  CodeMoveEventRecord* rec = &evt_rec.CodeMoveEventRecord_;
  rec->from = from->address();
  rec->to = to;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeDisableOptEvent(AbstractCode* code,
                                           SharedFunctionInfo* shared) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DISABLE_OPT);
  CodeDisableOptEventRecord* rec = &evt_rec.CodeDisableOptEventRecord_;
  rec->start = code->address();
  rec->bailout_reason = GetBailoutReason(shared->disable_optimization_reason());
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeDeoptEvent(Code* code, Address pc,
                                      int fp_to_sp_delta) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DEOPT);
  CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(code, pc);
  rec->start = code->address();
  rec->deopt_reason = DeoptimizeReasonToString(info.deopt_reason);
  rec->position = info.position;
  rec->deopt_id = info.deopt_id;
  rec->pc = reinterpret_cast<void*>(pc);
  rec->fp_to_sp_delta = fp_to_sp_delta;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::GetterCallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry =
      NewCodeEntry(CodeEventListener::CALLBACK_TAG, GetName(name), "get ");
  rec->size = 1;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::RegExpCodeCreateEvent(AbstractCode* code,
                                             String* source) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = NewCodeEntry(
      CodeEventListener::REG_EXP_TAG, GetName(source), "RegExp: ",
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  rec->size = code->ExecutableSize();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::SetterCallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry =
      NewCodeEntry(CodeEventListener::CALLBACK_TAG, GetName(name), "set ");
  rec->size = 1;
  DispatchCodeEvent(evt_rec);
}

Name* ProfilerListener::InferScriptName(Name* name, SharedFunctionInfo* info) {
  if (name->IsString() && String::cast(name)->length()) return name;
  if (!info->script()->IsScript()) return name;
  Object* source_url = Script::cast(info->script())->source_url();
  return source_url->IsName() ? Name::cast(source_url) : name;
}

void ProfilerListener::RecordInliningInfo(CodeEntry* entry,
                                          AbstractCode* abstract_code) {
  if (!abstract_code->IsCode()) return;
  Code* code = abstract_code->GetCode();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;
  DeoptimizationInputData* deopt_input_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  int deopt_count = deopt_input_data->DeoptCount();
  for (int i = 0; i < deopt_count; i++) {
    int pc_offset = deopt_input_data->Pc(i)->value();
    if (pc_offset == -1) continue;
    int translation_index = deopt_input_data->TranslationIndex(i)->value();
    TranslationIterator it(deopt_input_data->TranslationByteArray(),
                           translation_index);
    Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
    DCHECK_EQ(Translation::BEGIN, opcode);
    it.Skip(Translation::NumberOfOperandsFor(opcode));
    int depth = 0;
    std::vector<CodeEntry*> inline_stack;
    while (it.HasNext() &&
           Translation::BEGIN !=
               (opcode = static_cast<Translation::Opcode>(it.Next()))) {
      if (opcode != Translation::JS_FRAME &&
          opcode != Translation::INTERPRETED_FRAME) {
        it.Skip(Translation::NumberOfOperandsFor(opcode));
        continue;
      }
      it.Next();  // Skip ast_id
      int shared_info_id = it.Next();
      it.Next();  // Skip height
      SharedFunctionInfo* shared_info = SharedFunctionInfo::cast(
          deopt_input_data->LiteralArray()->get(shared_info_id));
      if (!depth++) continue;  // Skip the current function itself.
      CodeEntry* inline_entry = new CodeEntry(
          entry->tag(), GetFunctionName(shared_info->DebugName()),
          CodeEntry::kEmptyNamePrefix, entry->resource_name(),
          CpuProfileNode::kNoLineNumberInfo,
          CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
      inline_entry->FillFunctionInfo(shared_info);
      inline_stack.push_back(inline_entry);
    }
    if (!inline_stack.empty()) {
      entry->AddInlineStack(pc_offset, inline_stack);
      DCHECK(inline_stack.empty());
    }
  }
}

void ProfilerListener::RecordDeoptInlinedFrames(CodeEntry* entry,
                                                AbstractCode* abstract_code) {
  if (abstract_code->kind() != AbstractCode::OPTIMIZED_FUNCTION) return;
  Code* code = abstract_code->GetCode();
  DeoptimizationInputData* deopt_input_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  int const mask = RelocInfo::ModeMask(RelocInfo::DEOPT_ID);
  for (RelocIterator rit(code, mask); !rit.done(); rit.next()) {
    RelocInfo* reloc_info = rit.rinfo();
    DCHECK(RelocInfo::IsDeoptId(reloc_info->rmode()));
    int deopt_id = static_cast<int>(reloc_info->data());
    int translation_index =
        deopt_input_data->TranslationIndex(deopt_id)->value();
    TranslationIterator it(deopt_input_data->TranslationByteArray(),
                           translation_index);
    Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
    DCHECK_EQ(Translation::BEGIN, opcode);
    it.Skip(Translation::NumberOfOperandsFor(opcode));
    std::vector<CodeEntry::DeoptInlinedFrame> inlined_frames;
    while (it.HasNext() &&
           Translation::BEGIN !=
               (opcode = static_cast<Translation::Opcode>(it.Next()))) {
      if (opcode != Translation::JS_FRAME &&
          opcode != Translation::INTERPRETED_FRAME) {
        it.Skip(Translation::NumberOfOperandsFor(opcode));
        continue;
      }
      BailoutId ast_id = BailoutId(it.Next());
      int shared_info_id = it.Next();
      it.Next();  // Skip height
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(
          deopt_input_data->LiteralArray()->get(shared_info_id));
      int source_position;
      if (opcode == Translation::INTERPRETED_FRAME) {
        source_position =
            Deoptimizer::ComputeSourcePositionFromBytecodeArray(shared, ast_id);
      } else {
        DCHECK(opcode == Translation::JS_FRAME);
        source_position =
            Deoptimizer::ComputeSourcePositionFromBaselineCode(shared, ast_id);
      }
      int script_id = v8::UnboundScript::kNoScriptId;
      if (shared->script()->IsScript()) {
        Script* script = Script::cast(shared->script());
        script_id = script->id();
      }
      CodeEntry::DeoptInlinedFrame frame = {source_position, script_id};
      inlined_frames.push_back(frame);
    }
    if (!inlined_frames.empty() && !entry->HasDeoptInlinedFramesFor(deopt_id)) {
      entry->AddDeoptInlinedFrames(deopt_id, inlined_frames);
      DCHECK(inlined_frames.empty());
    }
  }
}

CodeEntry* ProfilerListener::NewCodeEntry(
    CodeEventListener::LogEventsAndTags tag, const char* name,
    const char* name_prefix, const char* resource_name, int line_number,
    int column_number, JITLineInfoTable* line_info, Address instruction_start) {
  CodeEntry* code_entry =
      new CodeEntry(tag, name, name_prefix, resource_name, line_number,
                    column_number, line_info, instruction_start);
  code_entries_.push_back(code_entry);
  return code_entry;
}

void ProfilerListener::AddObserver(CodeEventObserver* observer) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  if (std::find(observers_.begin(), observers_.end(), observer) !=
      observers_.end())
    return;
  observers_.push_back(observer);
}

void ProfilerListener::RemoveObserver(CodeEventObserver* observer) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  if (it == observers_.end()) return;
  observers_.erase(it);
}

}  // namespace internal
}  // namespace v8
