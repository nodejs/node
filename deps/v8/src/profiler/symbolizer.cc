// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/symbolizer.h"

#include "src/execution/vm-state.h"
#include "src/profiler/profile-generator.h"
#include "src/profiler/profiler-stats.h"
#include "src/profiler/tick-sample.h"

namespace v8 {
namespace internal {

Symbolizer::Symbolizer(InstructionStreamMap* instruction_stream_map)
    : code_map_(instruction_stream_map) {}

CodeEntry* Symbolizer::FindEntry(Address address,
                                 Address* out_instruction_start) {
  return code_map_->FindEntry(address, out_instruction_start);
}

namespace {

CodeEntry* EntryForVMState(StateTag tag) {
  switch (tag) {
    case GC:
      return CodeEntry::gc_entry();
    case JS:
    case PARSER:
    case COMPILER:
    case BYTECODE_COMPILER:
    case ATOMICS_WAIT:
    // DOM events handlers are reported as OTHER / EXTERNAL entries.
    // To avoid confusing people, let's put all these entries into
    // one bucket.
    case OTHER:
    case EXTERNAL:
    case LOGGING:
      return CodeEntry::program_entry();
    case IDLE:
      return CodeEntry::idle_entry();
  }
}

}  // namespace

Symbolizer::SymbolizedSample Symbolizer::SymbolizeTickSample(
    const TickSample& sample) {
  ProfileStackTrace stack_trace;
  // Conservatively reserve space for stack frames + pc + function + vm-state.
  // There could in fact be more of them because of inlined entries.
  stack_trace.reserve(sample.frames_count + 3);

  // The ProfileNode knows nothing about all versions of generated code for
  // the same JS function. The position information associated with
  // the latest version of generated code is used to find a source position
  // for a JS function. Then, the detected source position is passed to
  // ProfileNode to increase the tick count for this source position.
  LineAndColumn src_pos = {};
  bool src_pos_not_found = true;

  if (sample.pc != nullptr) {
    if (sample.has_external_callback && sample.state == EXTERNAL) {
      // Don't use PC when in external callback code, as it can point
      // inside a callback's code, and we will erroneously report
      // that a callback calls itself.
      stack_trace.push_back(
          {FindEntry(reinterpret_cast<Address>(sample.external_callback_entry)),
           LineAndColumn{}});
    } else {
      Address attributed_pc = reinterpret_cast<Address>(sample.pc);
      Address pc_entry_instruction_start = kNullAddress;
      CodeEntry* pc_entry =
          FindEntry(attributed_pc, &pc_entry_instruction_start);
      // If there is no pc_entry, we're likely in native code. Find out if the
      // top of the stack (the return address) was pointing inside a JS
      // function, meaning that we have encountered a frameless invocation.
      if (!pc_entry && !sample.has_external_callback) {
        attributed_pc = reinterpret_cast<Address>(sample.tos);
        pc_entry = FindEntry(attributed_pc, &pc_entry_instruction_start);
      }
      // If pc is in the function code before it set up stack frame or after the
      // frame was destroyed, StackFrameIteratorForProfiler incorrectly thinks
      // that ebp contains the return address of the current function and skips
      // the caller's frame. Check for this case and just skip such samples.
      if (pc_entry) {
        int pc_offset =
            static_cast<int>(attributed_pc - pc_entry_instruction_start);
        // TODO(petermarshall): pc_offset can still be negative in some cases.
        src_pos = pc_entry->GetSourcePosition(pc_offset);
        if (src_pos.line == v8::CpuProfileNode::kNoLineNumberInfo) {
          src_pos = pc_entry->line_and_column();
        }
        src_pos_not_found = false;
        stack_trace.push_back({pc_entry, src_pos});

        if (pc_entry->builtin() == Builtin::kFunctionPrototypeApply ||
            pc_entry->builtin() == Builtin::kFunctionPrototypeCall) {
          // When current function is either the Function.prototype.apply or the
          // Function.prototype.call builtin the top frame is either frame of
          // the calling JS function or internal frame.
          // In the latter case we know the caller for sure but in the
          // former case we don't so we simply replace the frame with
          // 'unresolved' entry.
          if (!sample.has_external_callback) {
            ProfilerStats::Instance()->AddReason(
                ProfilerStats::Reason::kInCallOrApply);
            stack_trace.push_back(
                {CodeEntry::unresolved_entry(), LineAndColumn{}});
          }
        }
      }
    }

    for (unsigned i = 0; i < sample.frames_count; ++i) {
      Address stack_pos = reinterpret_cast<Address>(sample.stack[i]);
      Address instruction_start = kNullAddress;
      CodeEntry* entry = FindEntry(stack_pos, &instruction_start);
      LineAndColumn position = {};
      if (entry) {
        // Find out if the entry has an inlining stack associated.
        int pc_offset = static_cast<int>(stack_pos - instruction_start);
        // TODO(petermarshall): pc_offset can still be negative in some cases.
        const std::vector<CodeEntryAndPosition>* inline_stack =
            entry->GetInlineStack(pc_offset);
        if (inline_stack) {
          LineAndColumn most_inlined_frame_pos =
              entry->GetSourcePosition(pc_offset);
          for (auto inline_stack_entry : *inline_stack) {
            stack_trace.push_back(inline_stack_entry);
          }

          // This is a bit of a messy hack. The position for the most-inlined
          // frame (the function at the end of the chain of function calls) has
          // the wrong position in inline_stack. The actual position in
          // this function is stored in the SourcePositionTable in entry. We fix
          // up the position for the most-inlined frame here.
          // TODO(petermarshall): Remove this and use a tree with a node per
          // inlining_id.
          DCHECK(!inline_stack->empty());
          size_t index = stack_trace.size() - inline_stack->size();
          stack_trace[index].line_and_column = most_inlined_frame_pos;
        }
        // Skip unresolved frames (e.g. internal frame) and get source position
        // of the first JS caller.
        if (src_pos_not_found) {
          src_pos = entry->GetSourcePosition(pc_offset);
          if (src_pos.line == v8::CpuProfileNode::kNoLineNumberInfo) {
            src_pos = entry->line_and_column();
          }
          src_pos_not_found = false;
        }
        position = entry->GetSourcePosition(pc_offset);

        // The inline stack contains the top-level function i.e. the same
        // function as entry. We don't want to add it twice. The one from the
        // inline stack has the correct position for this particular inlining
        // so we use it instead of pushing entry to stack_trace.
        if (inline_stack) continue;
      }
      stack_trace.push_back({entry, position});
    }
  }

  if (v8_flags.prof_browser_mode) {
    bool no_symbolized_entries = true;
    for (auto e : stack_trace) {
      if (e.code_entry != nullptr) {
        no_symbolized_entries = false;
        break;
      }
    }
    // If no frames were symbolized, put the VM state entry in.
    if (no_symbolized_entries) {
      if (sample.pc == nullptr) {
        ProfilerStats::Instance()->AddReason(ProfilerStats::Reason::kNullPC);
      } else {
        ProfilerStats::Instance()->AddReason(
            ProfilerStats::Reason::kNoSymbolizedFrames);
      }
      stack_trace.push_back({EntryForVMState(sample.state), LineAndColumn{}});
    }
  }

  return SymbolizedSample{stack_trace, src_pos};
}

}  // namespace internal
}  // namespace v8
