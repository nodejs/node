// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/profiler-listener.h"

#include <algorithm>

#include "src/base/vector.h"
#include "src/codegen/reloc-info.h"
#include "src/codegen/source-position-table.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/handles/handles-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/code.h"
#include "src/objects/objects-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/profiler/profile-generator-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

ProfilerListener::ProfilerListener(Isolate* isolate,
                                   CodeEventObserver* observer,
                                   CodeEntryStorage& code_entry_storage,
                                   WeakCodeRegistry& weak_code_registry,
                                   CpuProfilingNamingMode naming_mode)
    : isolate_(isolate),
      observer_(observer),
      code_entries_(code_entry_storage),
      weak_code_registry_(weak_code_registry),
      naming_mode_(naming_mode) {}

ProfilerListener::~ProfilerListener() = default;

void ProfilerListener::CodeCreateEvent(LogEventsAndTags tag,
                                       Handle<AbstractCode> code,
                                       const char* name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = code->InstructionStart();
  rec->entry =
      code_entries_.Create(tag, GetName(name), CodeEntry::kEmptyResourceName,
                           CpuProfileNode::kNoLineNumberInfo,
                           CpuProfileNode::kNoColumnNumberInfo, nullptr);
  rec->instruction_size = code->InstructionSize();
  weak_code_registry_.Track(rec->entry, code);
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(LogEventsAndTags tag,
                                       Handle<AbstractCode> code,
                                       Handle<Name> name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = code->InstructionStart();
  rec->entry =
      code_entries_.Create(tag, GetName(*name), CodeEntry::kEmptyResourceName,
                           CpuProfileNode::kNoLineNumberInfo,
                           CpuProfileNode::kNoColumnNumberInfo, nullptr);
  rec->instruction_size = code->InstructionSize();
  weak_code_registry_.Track(rec->entry, code);
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeCreateEvent(LogEventsAndTags tag,
                                       Handle<AbstractCode> code,
                                       Handle<SharedFunctionInfo> shared,
                                       Handle<Name> script_name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = code->InstructionStart();
  rec->entry =
      code_entries_.Create(tag, GetName(shared->DebugNameCStr().get()),
                           GetName(InferScriptName(*script_name, *shared)),
                           CpuProfileNode::kNoLineNumberInfo,
                           CpuProfileNode::kNoColumnNumberInfo, nullptr);
  DCHECK_IMPLIES(code->IsCode(), code->kind() == CodeKind::BASELINE);
  rec->entry->FillFunctionInfo(*shared);
  rec->instruction_size = code->InstructionSize();
  weak_code_registry_.Track(rec->entry, code);
  DispatchCodeEvent(evt_rec);
}

namespace {

CodeEntry* GetOrInsertCachedEntry(
    std::unordered_set<CodeEntry*, CodeEntry::Hasher, CodeEntry::Equals>*
        entries,
    CodeEntry* search_value, CodeEntryStorage& storage) {
  auto it = entries->find(search_value);
  if (it != entries->end()) {
    storage.DecRef(search_value);
    return *it;
  }
  entries->insert(search_value);
  return search_value;
}

}  // namespace

void ProfilerListener::CodeCreateEvent(LogEventsAndTags tag,
                                       Handle<AbstractCode> abstract_code,
                                       Handle<SharedFunctionInfo> shared,
                                       Handle<Name> script_name, int line,
                                       int column) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = abstract_code->InstructionStart();
  std::unique_ptr<SourcePositionTable> line_table;
  std::unordered_map<int, std::vector<CodeEntryAndLineNumber>> inline_stacks;
  std::unordered_set<CodeEntry*, CodeEntry::Hasher, CodeEntry::Equals>
      cached_inline_entries;
  bool is_shared_cross_origin = false;
  if (shared->script().IsScript()) {
    Handle<Script> script = handle(Script::cast(shared->script()), isolate_);
    line_table.reset(new SourcePositionTable());

    is_shared_cross_origin = script->origin_options().IsSharedCrossOrigin();

    bool is_baseline = abstract_code->kind() == CodeKind::BASELINE;
    Handle<ByteArray> source_position_table(
        abstract_code->SourcePositionTable(*shared), isolate_);
    std::unique_ptr<baseline::BytecodeOffsetIterator> baseline_iterator =
        nullptr;
    if (is_baseline) {
      Handle<BytecodeArray> bytecodes(shared->GetBytecodeArray(isolate_),
                                      isolate_);
      Handle<ByteArray> bytecode_offsets(
          abstract_code->GetCode().bytecode_offset_table(), isolate_);
      baseline_iterator = std::make_unique<baseline::BytecodeOffsetIterator>(
          bytecode_offsets, bytecodes);
    }
    // Add each position to the source position table and store inlining stacks
    // for inline positions. We store almost the same information in the
    // profiler as is stored on the code object, except that we transform source
    // positions to line numbers here, because we only care about attributing
    // ticks to a given line.
    for (SourcePositionTableIterator it(source_position_table); !it.done();
         it.Advance()) {
      int position = it.source_position().ScriptOffset();
      int inlining_id = it.source_position().InliningId();
      int code_offset = it.code_offset();
      if (is_baseline) {
        // Use the bytecode offset to calculate pc offset for baseline code.
        baseline_iterator->AdvanceToBytecodeOffset(code_offset);
        code_offset =
            static_cast<int>(baseline_iterator->current_pc_start_offset());
      }

      if (inlining_id == SourcePosition::kNotInlined) {
        int line_number = script->GetLineNumber(position) + 1;
        line_table->SetPosition(code_offset, line_number, inlining_id);
      } else {
        DCHECK(!is_baseline);
        DCHECK(abstract_code->IsCode());
        Handle<Code> code = handle(abstract_code->GetCode(), isolate_);
        std::vector<SourcePositionInfo> stack =
            it.source_position().InliningStack(code);
        DCHECK(!stack.empty());

        // When we have an inlining id and we are doing cross-script inlining,
        // then the script of the inlined frames may be different to the script
        // of |shared|.
        int line_number = stack.front().line + 1;
        line_table->SetPosition(code_offset, line_number, inlining_id);

        std::vector<CodeEntryAndLineNumber> inline_stack;
        for (SourcePositionInfo& pos_info : stack) {
          if (pos_info.position.ScriptOffset() == kNoSourcePosition) continue;
          if (pos_info.script.is_null()) continue;

          int line_number =
              pos_info.script->GetLineNumber(pos_info.position.ScriptOffset()) +
              1;

          const char* resource_name =
              (pos_info.script->name().IsName())
                  ? GetName(Name::cast(pos_info.script->name()))
                  : CodeEntry::kEmptyResourceName;

          bool inline_is_shared_cross_origin =
              pos_info.script->origin_options().IsSharedCrossOrigin();

          // We need the start line number and column number of the function for
          // kLeafNodeLineNumbers mode. Creating a SourcePositionInfo is a handy
          // way of getting both easily.
          SourcePositionInfo start_pos_info(
              SourcePosition(pos_info.shared->StartPosition()),
              pos_info.shared);

          CodeEntry* inline_entry = code_entries_.Create(
              tag, GetFunctionName(*pos_info.shared), resource_name,
              start_pos_info.line + 1, start_pos_info.column + 1, nullptr,
              inline_is_shared_cross_origin);
          inline_entry->FillFunctionInfo(*pos_info.shared);

          // Create a canonical CodeEntry for each inlined frame and then re-use
          // them for subsequent inline stacks to avoid a lot of duplication.
          CodeEntry* cached_entry = GetOrInsertCachedEntry(
              &cached_inline_entries, inline_entry, code_entries_);

          inline_stack.push_back({cached_entry, line_number});
        }
        DCHECK(!inline_stack.empty());
        inline_stacks.emplace(inlining_id, std::move(inline_stack));
      }
    }
  }
  rec->entry = code_entries_.Create(
      tag, GetFunctionName(*shared),
      GetName(InferScriptName(*script_name, *shared)), line, column,
      std::move(line_table), is_shared_cross_origin);
  if (!inline_stacks.empty()) {
    rec->entry->SetInlineStacks(std::move(cached_inline_entries),
                                std::move(inline_stacks));
  }

  rec->entry->FillFunctionInfo(*shared);
  rec->instruction_size = abstract_code->InstructionSize();
  weak_code_registry_.Track(rec->entry, abstract_code);
  DispatchCodeEvent(evt_rec);
}

#if V8_ENABLE_WEBASSEMBLY
void ProfilerListener::CodeCreateEvent(LogEventsAndTags tag,
                                       const wasm::WasmCode* code,
                                       wasm::WasmName name,
                                       const char* source_url, int code_offset,
                                       int script_id) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = code->instruction_start();
  rec->entry = code_entries_.Create(tag, GetName(name), GetName(source_url), 1,
                                    code_offset + 1, nullptr, true,
                                    CodeEntry::CodeType::WASM);
  rec->entry->set_script_id(script_id);
  rec->entry->set_position(code_offset);
  rec->instruction_size = code->instructions().length();
  DispatchCodeEvent(evt_rec);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void ProfilerListener::CallbackEvent(Handle<Name> name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = entry_point;
  rec->entry =
      code_entries_.Create(CodeEventListener::CALLBACK_TAG, GetName(*name));
  rec->instruction_size = 1;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::GetterCallbackEvent(Handle<Name> name,
                                           Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = entry_point;
  rec->entry = code_entries_.Create(CodeEventListener::CALLBACK_TAG,
                                    GetConsName("get ", *name));
  rec->instruction_size = 1;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::SetterCallbackEvent(Handle<Name> name,
                                           Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = entry_point;
  rec->entry = code_entries_.Create(CodeEventListener::CALLBACK_TAG,
                                    GetConsName("set ", *name));
  rec->instruction_size = 1;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                             Handle<String> source) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->instruction_start = code->InstructionStart();
  rec->entry = code_entries_.Create(
      CodeEventListener::REG_EXP_TAG, GetConsName("RegExp: ", *source),
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, nullptr);
  rec->instruction_size = code->InstructionSize();
  weak_code_registry_.Track(rec->entry, code);
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  DisallowGarbageCollection no_gc;
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_MOVE);
  CodeMoveEventRecord* rec = &evt_rec.CodeMoveEventRecord_;
  rec->from_instruction_start = from.InstructionStart();
  rec->to_instruction_start = to.InstructionStart();
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::NativeContextMoveEvent(Address from, Address to) {
  CodeEventsContainer evt_rec(CodeEventRecord::NATIVE_CONTEXT_MOVE);
  evt_rec.NativeContextMoveEventRecord_.from_address = from;
  evt_rec.NativeContextMoveEventRecord_.to_address = to;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeDisableOptEvent(Handle<AbstractCode> code,
                                           Handle<SharedFunctionInfo> shared) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DISABLE_OPT);
  CodeDisableOptEventRecord* rec = &evt_rec.CodeDisableOptEventRecord_;
  rec->instruction_start = code->InstructionStart();
  rec->bailout_reason = GetBailoutReason(shared->disable_optimization_reason());
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind,
                                      Address pc, int fp_to_sp_delta,
                                      bool reuse_code) {
  // When reuse_code is true it is just a bailout and not an actual deopt.
  if (reuse_code) return;
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DEOPT);
  CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(*code, pc);
  rec->instruction_start = code->InstructionStart();
  rec->deopt_reason = DeoptimizeReasonToString(info.deopt_reason);
  rec->deopt_id = info.deopt_id;
  rec->pc = pc;
  rec->fp_to_sp_delta = fp_to_sp_delta;

  // When a function is deoptimized, we store the deoptimized frame information
  // for the use of GetDeoptInfos().
  AttachDeoptInlinedFrames(code, rec);
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::WeakCodeClearEvent() { weak_code_registry_.Sweep(this); }

void ProfilerListener::OnHeapObjectDeletion(CodeEntry* entry) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DELETE);
  evt_rec.CodeDeleteEventRecord_.entry = entry;
  DispatchCodeEvent(evt_rec);
}

void ProfilerListener::CodeSweepEvent() { weak_code_registry_.Sweep(this); }

const char* ProfilerListener::GetName(base::Vector<const char> name) {
  // TODO(all): Change {StringsStorage} to accept non-null-terminated strings.
  base::OwnedVector<char> null_terminated =
      base::OwnedVector<char>::New(name.size() + 1);
  std::copy(name.begin(), name.end(), null_terminated.begin());
  null_terminated[name.size()] = '\0';
  return GetName(null_terminated.begin());
}

Name ProfilerListener::InferScriptName(Name name, SharedFunctionInfo info) {
  if (name.IsString() && String::cast(name).length()) return name;
  if (!info.script().IsScript()) return name;
  Object source_url = Script::cast(info.script()).source_url();
  return source_url.IsName() ? Name::cast(source_url) : name;
}

const char* ProfilerListener::GetFunctionName(SharedFunctionInfo shared) {
  switch (naming_mode_) {
    case kDebugNaming:
      return GetName(shared.DebugNameCStr().get());
    case kStandardNaming:
      return GetName(shared.Name());
    default:
      UNREACHABLE();
  }
}

void ProfilerListener::AttachDeoptInlinedFrames(Handle<Code> code,
                                                CodeDeoptEventRecord* rec) {
  int deopt_id = rec->deopt_id;
  SourcePosition last_position = SourcePosition::Unknown();
  int mask = RelocInfo::ModeMask(RelocInfo::DEOPT_ID) |
             RelocInfo::ModeMask(RelocInfo::DEOPT_SCRIPT_OFFSET) |
             RelocInfo::ModeMask(RelocInfo::DEOPT_INLINING_ID);

  rec->deopt_frames = nullptr;
  rec->deopt_frame_count = 0;

  for (RelocIterator it(*code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::DEOPT_SCRIPT_OFFSET) {
      int script_offset = static_cast<int>(info->data());
      it.next();
      DCHECK(it.rinfo()->rmode() == RelocInfo::DEOPT_INLINING_ID);
      int inlining_id = static_cast<int>(it.rinfo()->data());
      last_position = SourcePosition(script_offset, inlining_id);
      continue;
    }
    if (info->rmode() == RelocInfo::DEOPT_ID) {
      if (deopt_id != static_cast<int>(info->data())) continue;
      DCHECK(last_position.IsKnown());

      // SourcePosition::InliningStack allocates a handle for the SFI of each
      // frame. These don't escape this function, but quickly add up. This
      // scope limits their lifetime.
      HandleScope scope(isolate_);
      std::vector<SourcePositionInfo> stack = last_position.InliningStack(code);
      CpuProfileDeoptFrame* deopt_frames =
          new CpuProfileDeoptFrame[stack.size()];

      int deopt_frame_count = 0;
      for (SourcePositionInfo& pos_info : stack) {
        if (pos_info.position.ScriptOffset() == kNoSourcePosition) continue;
        if (pos_info.script.is_null()) continue;
        int script_id = pos_info.script->id();
        size_t offset = static_cast<size_t>(pos_info.position.ScriptOffset());
        deopt_frames[deopt_frame_count++] = {script_id, offset};
      }
      rec->deopt_frames = deopt_frames;
      rec->deopt_frame_count = deopt_frame_count;
      break;
    }
  }
}

}  // namespace internal
}  // namespace v8
