// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-analysis.h"

#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-array-random-iterator.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

using interpreter::Bytecode;
using interpreter::Bytecodes;
using interpreter::OperandType;

BytecodeLoopAssignments::BytecodeLoopAssignments(int parameter_count,
                                                 int register_count, Zone* zone)
    : parameter_count_(parameter_count),
      bit_vector_(new (zone)
                      BitVector(parameter_count + register_count, zone)) {}

void BytecodeLoopAssignments::Add(interpreter::Register r) {
  if (r.is_parameter()) {
    bit_vector_->Add(r.ToParameterIndex(parameter_count_));
  } else {
    bit_vector_->Add(parameter_count_ + r.index());
  }
}

void BytecodeLoopAssignments::AddList(interpreter::Register r, uint32_t count) {
  if (r.is_parameter()) {
    for (uint32_t i = 0; i < count; i++) {
      DCHECK(interpreter::Register(r.index() + i).is_parameter());
      bit_vector_->Add(r.ToParameterIndex(parameter_count_) + i);
    }
  } else {
    for (uint32_t i = 0; i < count; i++) {
      DCHECK(!interpreter::Register(r.index() + i).is_parameter());
      bit_vector_->Add(parameter_count_ + r.index() + i);
    }
  }
}


void BytecodeLoopAssignments::Union(const BytecodeLoopAssignments& other) {
  bit_vector_->Union(*other.bit_vector_);
}

bool BytecodeLoopAssignments::ContainsParameter(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, parameter_count());
  return bit_vector_->Contains(index);
}

bool BytecodeLoopAssignments::ContainsLocal(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, local_count());
  return bit_vector_->Contains(parameter_count_ + index);
}

ResumeJumpTarget::ResumeJumpTarget(int suspend_id, int target_offset,
                                   int final_target_offset)
    : suspend_id_(suspend_id),
      target_offset_(target_offset),
      final_target_offset_(final_target_offset) {}

ResumeJumpTarget ResumeJumpTarget::Leaf(int suspend_id, int target_offset) {
  return ResumeJumpTarget(suspend_id, target_offset, target_offset);
}

ResumeJumpTarget ResumeJumpTarget::AtLoopHeader(int loop_header_offset,
                                                const ResumeJumpTarget& next) {
  return ResumeJumpTarget(next.suspend_id(), loop_header_offset,
                          next.target_offset());
}

BytecodeAnalysis::BytecodeAnalysis(Handle<BytecodeArray> bytecode_array,
                                   Zone* zone, bool do_liveness_analysis)
    : bytecode_array_(bytecode_array),
      do_liveness_analysis_(do_liveness_analysis),
      zone_(zone),
      loop_stack_(zone),
      loop_end_index_queue_(zone),
      resume_jump_targets_(zone),
      end_to_header_(zone),
      header_to_info_(zone),
      osr_entry_point_(-1),
      liveness_map_(bytecode_array->length(), zone) {}

namespace {

void UpdateInLiveness(Bytecode bytecode, BytecodeLivenessState& in_liveness,
                      const interpreter::BytecodeArrayAccessor& accessor) {
  int num_operands = Bytecodes::NumberOfOperands(bytecode);
  const OperandType* operand_types = Bytecodes::GetOperandTypes(bytecode);

  // Special case Suspend and Resume to just pass through liveness.
  if (bytecode == Bytecode::kSuspendGenerator) {
    // The generator object has to be live.
    in_liveness.MarkRegisterLive(accessor.GetRegisterOperand(0).index());
    // Suspend additionally reads and returns the accumulator
    DCHECK(Bytecodes::ReadsAccumulator(bytecode));
    in_liveness.MarkAccumulatorLive();
    return;
  }
  if (bytecode == Bytecode::kResumeGenerator) {
    // The generator object has to be live.
    in_liveness.MarkRegisterLive(accessor.GetRegisterOperand(0).index());
    return;
  }

  if (Bytecodes::WritesAccumulator(bytecode)) {
    in_liveness.MarkAccumulatorDead();
  }
  for (int i = 0; i < num_operands; ++i) {
    switch (operand_types[i]) {
      case OperandType::kRegOut: {
        interpreter::Register r = accessor.GetRegisterOperand(i);
        if (!r.is_parameter()) {
          in_liveness.MarkRegisterDead(r.index());
        }
        break;
      }
      case OperandType::kRegOutList: {
        interpreter::Register r = accessor.GetRegisterOperand(i++);
        uint32_t reg_count = accessor.GetRegisterCountOperand(i);
        if (!r.is_parameter()) {
          for (uint32_t j = 0; j < reg_count; ++j) {
            DCHECK(!interpreter::Register(r.index() + j).is_parameter());
            in_liveness.MarkRegisterDead(r.index() + j);
          }
        }
        break;
      }
      case OperandType::kRegOutPair: {
        interpreter::Register r = accessor.GetRegisterOperand(i);
        if (!r.is_parameter()) {
          DCHECK(!interpreter::Register(r.index() + 1).is_parameter());
          in_liveness.MarkRegisterDead(r.index());
          in_liveness.MarkRegisterDead(r.index() + 1);
        }
        break;
      }
      case OperandType::kRegOutTriple: {
        interpreter::Register r = accessor.GetRegisterOperand(i);
        if (!r.is_parameter()) {
          DCHECK(!interpreter::Register(r.index() + 1).is_parameter());
          DCHECK(!interpreter::Register(r.index() + 2).is_parameter());
          in_liveness.MarkRegisterDead(r.index());
          in_liveness.MarkRegisterDead(r.index() + 1);
          in_liveness.MarkRegisterDead(r.index() + 2);
        }
        break;
      }
      default:
        DCHECK(!Bytecodes::IsRegisterOutputOperandType(operand_types[i]));
        break;
    }
  }

  if (Bytecodes::ReadsAccumulator(bytecode)) {
    in_liveness.MarkAccumulatorLive();
  }
  for (int i = 0; i < num_operands; ++i) {
    switch (operand_types[i]) {
      case OperandType::kReg: {
        interpreter::Register r = accessor.GetRegisterOperand(i);
        if (!r.is_parameter()) {
          in_liveness.MarkRegisterLive(r.index());
        }
        break;
      }
      case OperandType::kRegPair: {
        interpreter::Register r = accessor.GetRegisterOperand(i);
        if (!r.is_parameter()) {
          DCHECK(!interpreter::Register(r.index() + 1).is_parameter());
          in_liveness.MarkRegisterLive(r.index());
          in_liveness.MarkRegisterLive(r.index() + 1);
        }
        break;
      }
      case OperandType::kRegList: {
        interpreter::Register r = accessor.GetRegisterOperand(i++);
        uint32_t reg_count = accessor.GetRegisterCountOperand(i);
        if (!r.is_parameter()) {
          for (uint32_t j = 0; j < reg_count; ++j) {
            DCHECK(!interpreter::Register(r.index() + j).is_parameter());
            in_liveness.MarkRegisterLive(r.index() + j);
          }
        }
        break;
      }
      default:
        DCHECK(!Bytecodes::IsRegisterInputOperandType(operand_types[i]));
        break;
    }
  }
}

void UpdateOutLiveness(Bytecode bytecode, BytecodeLivenessState& out_liveness,
                       BytecodeLivenessState* next_bytecode_in_liveness,
                       const interpreter::BytecodeArrayAccessor& accessor,
                       const BytecodeLivenessMap& liveness_map) {
  int current_offset = accessor.current_offset();
  const Handle<BytecodeArray>& bytecode_array = accessor.bytecode_array();

  // Special case Suspend and Resume to just pass through liveness.
  if (bytecode == Bytecode::kSuspendGenerator ||
      bytecode == Bytecode::kResumeGenerator) {
    out_liveness.Union(*next_bytecode_in_liveness);
    return;
  }

  // Update from jump target (if any). Skip loops, we update these manually in
  // the liveness iterations.
  if (Bytecodes::IsForwardJump(bytecode)) {
    int target_offset = accessor.GetJumpTargetOffset();
    out_liveness.Union(*liveness_map.GetInLiveness(target_offset));
  } else if (Bytecodes::IsSwitch(bytecode)) {
    for (const auto& entry : accessor.GetJumpTableTargetOffsets()) {
      out_liveness.Union(*liveness_map.GetInLiveness(entry.target_offset));
    }
  }

  // Update from next bytecode (unless there isn't one or this is an
  // unconditional jump).
  if (next_bytecode_in_liveness != nullptr &&
      !Bytecodes::IsUnconditionalJump(bytecode)) {
    out_liveness.Union(*next_bytecode_in_liveness);
  }

  // Update from exception handler (if any).
  if (!interpreter::Bytecodes::IsWithoutExternalSideEffects(bytecode)) {
    int handler_context;
    // TODO(leszeks): We should look up this range only once per entry.
    HandlerTable table(*bytecode_array);
    int handler_offset =
        table.LookupRange(current_offset, &handler_context, nullptr);

    if (handler_offset != -1) {
      bool was_accumulator_live = out_liveness.AccumulatorIsLive();
      out_liveness.Union(*liveness_map.GetInLiveness(handler_offset));
      out_liveness.MarkRegisterLive(handler_context);
      if (!was_accumulator_live) {
        // The accumulator is reset to the exception on entry into a handler,
        // and so shouldn't be considered live coming out of this bytecode just
        // because it's live coming into the handler. So, kill the accumulator
        // if the handler is the only thing that made it live.
        out_liveness.MarkAccumulatorDead();

        // TODO(leszeks): Ideally the accumulator wouldn't be considered live at
        // the start of the handler, but looking up if the current bytecode is
        // the start of a handler is not free, so we should only do it if we
        // decide it's necessary.
      }
    }
  }
}

void UpdateLiveness(Bytecode bytecode, BytecodeLiveness& liveness,
                    BytecodeLivenessState** next_bytecode_in_liveness,
                    const interpreter::BytecodeArrayAccessor& accessor,
                    const BytecodeLivenessMap& liveness_map) {
  UpdateOutLiveness(bytecode, *liveness.out, *next_bytecode_in_liveness,
                    accessor, liveness_map);
  liveness.in->CopyFrom(*liveness.out);
  UpdateInLiveness(bytecode, *liveness.in, accessor);

  *next_bytecode_in_liveness = liveness.in;
}

void UpdateAssignments(Bytecode bytecode, BytecodeLoopAssignments& assignments,
                       const interpreter::BytecodeArrayAccessor& accessor) {
  int num_operands = Bytecodes::NumberOfOperands(bytecode);
  const OperandType* operand_types = Bytecodes::GetOperandTypes(bytecode);

  for (int i = 0; i < num_operands; ++i) {
    switch (operand_types[i]) {
      case OperandType::kRegOut: {
        assignments.Add(accessor.GetRegisterOperand(i));
        break;
      }
      case OperandType::kRegOutList: {
        interpreter::Register r = accessor.GetRegisterOperand(i++);
        uint32_t reg_count = accessor.GetRegisterCountOperand(i);
        assignments.AddList(r, reg_count);
        break;
      }
      case OperandType::kRegOutPair: {
        assignments.AddList(accessor.GetRegisterOperand(i), 2);
        break;
      }
      case OperandType::kRegOutTriple: {
        assignments.AddList(accessor.GetRegisterOperand(i), 3);
        break;
      }
      default:
        DCHECK(!Bytecodes::IsRegisterOutputOperandType(operand_types[i]));
        break;
    }
  }
}

}  // namespace

void BytecodeAnalysis::Analyze(BailoutId osr_bailout_id) {
  loop_stack_.push({-1, nullptr});

  BytecodeLivenessState* next_bytecode_in_liveness = nullptr;

  bool is_osr = !osr_bailout_id.IsNone();
  int osr_loop_end_offset = is_osr ? osr_bailout_id.ToInt() : -1;

  int generator_switch_index = -1;

  interpreter::BytecodeArrayRandomIterator iterator(bytecode_array(), zone());
  for (iterator.GoToEnd(); iterator.IsValid(); --iterator) {
    Bytecode bytecode = iterator.current_bytecode();
    int current_offset = iterator.current_offset();

    if (bytecode == Bytecode::kSwitchOnGeneratorState) {
      DCHECK_EQ(generator_switch_index, -1);
      generator_switch_index = iterator.current_index();
    }

    if (bytecode == Bytecode::kJumpLoop) {
      // Every byte up to and including the last byte within the backwards jump
      // instruction is considered part of the loop, set loop end accordingly.
      int loop_end = current_offset + iterator.current_bytecode_size();
      int loop_header = iterator.GetJumpTargetOffset();
      PushLoop(loop_header, loop_end);

      if (current_offset == osr_loop_end_offset) {
        osr_entry_point_ = loop_header;
      } else if (current_offset < osr_loop_end_offset) {
        // Check we've found the osr_entry_point if we've gone past the
        // osr_loop_end_offset. Note, we are iterating the bytecode in reverse,
        // so the less than in the check is correct.
        DCHECK_NE(-1, osr_entry_point_);
      }

      // Save the index so that we can do another pass later.
      if (do_liveness_analysis_) {
        loop_end_index_queue_.push_back(iterator.current_index());
      }
    } else if (loop_stack_.size() > 1) {
      LoopStackEntry& current_loop = loop_stack_.top();
      LoopInfo* current_loop_info = current_loop.loop_info;

      // TODO(leszeks): Ideally, we'd only set values that were assigned in
      // the loop *and* are live when the loop exits. However, this requires
      // tracking the out-liveness of *all* loop exits, which is not
      // information we currently have.
      UpdateAssignments(bytecode, current_loop_info->assignments(), iterator);

      // Update suspend counts for this loop, though only if not OSR.
      if (!is_osr && bytecode == Bytecode::kSuspendGenerator) {
        int suspend_id = iterator.GetUnsignedImmediateOperand(3);
        int resume_offset = current_offset + iterator.current_bytecode_size();
        current_loop_info->AddResumeTarget(
            ResumeJumpTarget::Leaf(suspend_id, resume_offset));
      }

      // If we've reached the header of the loop, pop it off the stack.
      if (current_offset == current_loop.header_offset) {
        loop_stack_.pop();
        if (loop_stack_.size() > 1) {
          // If there is still an outer loop, propagate inner loop assignments.
          LoopInfo* parent_loop_info = loop_stack_.top().loop_info;

          parent_loop_info->assignments().Union(
              current_loop_info->assignments());

          // Also, propagate resume targets. Instead of jumping to the target
          // itself, the outer loop will jump to this loop header for any
          // targets that are inside the current loop, so that this loop stays
          // reducible. Hence, a nested loop of the form:
          //
          //                switch (#1 -> suspend1, #2 -> suspend2)
          //                loop {
          //     suspend1:    suspend #1
          //                  loop {
          //     suspend2:      suspend #2
          //                  }
          //                }
          //
          // becomes:
          //
          //                switch (#1 -> loop1, #2 -> loop1)
          //     loop1:     loop {
          //                  switch (#1 -> suspend1, #2 -> loop2)
          //     suspend1:    suspend #1
          //     loop2:       loop {
          //                    switch (#2 -> suspend2)
          //     suspend2:      suspend #2
          //                  }
          //                }
          for (const auto& target : current_loop_info->resume_jump_targets()) {
            parent_loop_info->AddResumeTarget(
                ResumeJumpTarget::AtLoopHeader(current_offset, target));
          }

        } else {
          // Otherwise, just propagate inner loop suspends to top-level.
          for (const auto& target : current_loop_info->resume_jump_targets()) {
            resume_jump_targets_.push_back(
                ResumeJumpTarget::AtLoopHeader(current_offset, target));
          }
        }
      }
    } else if (!is_osr && bytecode == Bytecode::kSuspendGenerator) {
      // If we're not in a loop, we still need to look for suspends.
      // TODO(leszeks): It would be nice to de-duplicate this with the in-loop
      // case
      int suspend_id = iterator.GetUnsignedImmediateOperand(3);
      int resume_offset = current_offset + iterator.current_bytecode_size();
      resume_jump_targets_.push_back(
          ResumeJumpTarget::Leaf(suspend_id, resume_offset));
    }

    if (do_liveness_analysis_) {
      BytecodeLiveness& liveness = liveness_map_.InitializeLiveness(
          current_offset, bytecode_array()->register_count(), zone());
      UpdateLiveness(bytecode, liveness, &next_bytecode_in_liveness, iterator,
                     liveness_map_);
    }
  }

  DCHECK_EQ(loop_stack_.size(), 1u);
  DCHECK_EQ(loop_stack_.top().header_offset, -1);

  DCHECK(ResumeJumpTargetsAreValid());

  if (!do_liveness_analysis_) return;

  // At this point, every bytecode has a valid in and out liveness, except for
  // propagating liveness across back edges (i.e. JumpLoop). Subsequent liveness
  // analysis iterations can only add additional liveness bits that are pulled
  // across these back edges.
  //
  // Furthermore, a loop header's in-liveness can only change based on any
  // bytecodes *after* the loop end --  it cannot change as a result of the
  // JumpLoop liveness being updated, as the only liveness bits than can be
  // added to the loop body are those of the loop header.
  //
  // So, if we know that the liveness of bytecodes after a loop header won't
  // change (e.g. because there are no loops in them, or we have already ensured
  // those loops are valid), we can safely update the loop end and pass over the
  // loop body, and then never have to pass over that loop end again, because we
  // have shown that its target, the loop header, can't change from the entries
  // after the loop, and can't change from any loop body pass.
  //
  // This means that in a pass, we can iterate backwards over the bytecode
  // array, process any loops that we encounter, and on subsequent passes we can
  // skip processing those loops (though we still have to process inner loops).
  //
  // Equivalently, we can queue up loop ends from back to front, and pass over
  // the loops in that order, as this preserves both the bottom-to-top and
  // outer-to-inner requirements.

  for (int loop_end_index : loop_end_index_queue_) {
    iterator.GoToIndex(loop_end_index);

    DCHECK_EQ(iterator.current_bytecode(), Bytecode::kJumpLoop);

    int header_offset = iterator.GetJumpTargetOffset();
    int end_offset = iterator.current_offset();

    BytecodeLiveness& header_liveness =
        liveness_map_.GetLiveness(header_offset);
    BytecodeLiveness& end_liveness = liveness_map_.GetLiveness(end_offset);

    if (!end_liveness.out->UnionIsChanged(*header_liveness.in)) {
      // Only update the loop body if the loop end liveness changed.
      continue;
    }
    end_liveness.in->CopyFrom(*end_liveness.out);
    next_bytecode_in_liveness = end_liveness.in;

    // Advance into the loop body.
    --iterator;
    for (; iterator.current_offset() > header_offset; --iterator) {
      Bytecode bytecode = iterator.current_bytecode();
      int current_offset = iterator.current_offset();
      BytecodeLiveness& liveness = liveness_map_.GetLiveness(current_offset);

      UpdateLiveness(bytecode, liveness, &next_bytecode_in_liveness, iterator,
                     liveness_map_);
    }
    // Now we are at the loop header. Since the in-liveness of the header
    // can't change, we need only to update the out-liveness.
    UpdateOutLiveness(iterator.current_bytecode(), *header_liveness.out,
                      next_bytecode_in_liveness, iterator, liveness_map_);
  }

  // Process the generator switch statement separately, once the loops are done.
  // This has to be a separate pass because the generator switch can jump into
  // the middle of loops (and is the only kind of jump that can jump across a
  // loop header).
  if (generator_switch_index != -1) {
    iterator.GoToIndex(generator_switch_index);
    DCHECK_EQ(iterator.current_bytecode(), Bytecode::kSwitchOnGeneratorState);

    int current_offset = iterator.current_offset();
    BytecodeLiveness& switch_liveness =
        liveness_map_.GetLiveness(current_offset);

    bool any_changed = false;
    for (const auto& entry : iterator.GetJumpTableTargetOffsets()) {
      if (switch_liveness.out->UnionIsChanged(
              *liveness_map_.GetInLiveness(entry.target_offset))) {
        any_changed = true;
      }
    }

    // If the switch liveness changed, we have to propagate it up the remaining
    // bytecodes before it.
    if (any_changed) {
      switch_liveness.in->CopyFrom(*switch_liveness.out);
      UpdateInLiveness(Bytecode::kSwitchOnGeneratorState, *switch_liveness.in,
                       iterator);
      next_bytecode_in_liveness = switch_liveness.in;
      for (--iterator; iterator.IsValid(); --iterator) {
        Bytecode bytecode = iterator.current_bytecode();
        int current_offset = iterator.current_offset();
        BytecodeLiveness& liveness = liveness_map_.GetLiveness(current_offset);

        // There shouldn't be any more loops.
        DCHECK_NE(bytecode, Bytecode::kJumpLoop);

        UpdateLiveness(bytecode, liveness, &next_bytecode_in_liveness, iterator,
                       liveness_map_);
      }
    }
  }

  DCHECK(LivenessIsValid());
}

void BytecodeAnalysis::PushLoop(int loop_header, int loop_end) {
  DCHECK(loop_header < loop_end);
  DCHECK(loop_stack_.top().header_offset < loop_header);
  DCHECK(end_to_header_.find(loop_end) == end_to_header_.end());
  DCHECK(header_to_info_.find(loop_header) == header_to_info_.end());

  int parent_offset = loop_stack_.top().header_offset;

  end_to_header_.insert({loop_end, loop_header});
  auto it = header_to_info_.insert(
      {loop_header, LoopInfo(parent_offset, bytecode_array_->parameter_count(),
                             bytecode_array_->register_count(), zone_)});
  // Get the loop info pointer from the output of insert.
  LoopInfo* loop_info = &it.first->second;

  loop_stack_.push({loop_header, loop_info});
}

bool BytecodeAnalysis::IsLoopHeader(int offset) const {
  return header_to_info_.find(offset) != header_to_info_.end();
}

int BytecodeAnalysis::GetLoopOffsetFor(int offset) const {
  auto loop_end_to_header = end_to_header_.upper_bound(offset);
  // If there is no next end => offset is not in a loop.
  if (loop_end_to_header == end_to_header_.end()) {
    return -1;
  }
  // If the header precedes the offset, this is the loop
  //
  //   .> header  <--loop_end_to_header
  //   |
  //   |  <--offset
  //   |
  //   `- end
  if (loop_end_to_header->second <= offset) {
    return loop_end_to_header->second;
  }
  // Otherwise there is a (potentially nested) loop after this offset.
  //
  //    <--offset
  //
  //   .> header
  //   |
  //   | .> header  <--loop_end_to_header
  //   | |
  //   | `- end
  //   |
  //   `- end
  // We just return the parent of the next loop (might be -1).
  DCHECK(header_to_info_.upper_bound(offset) != header_to_info_.end());

  return header_to_info_.upper_bound(offset)->second.parent_offset();
}

const LoopInfo& BytecodeAnalysis::GetLoopInfoFor(int header_offset) const {
  DCHECK(IsLoopHeader(header_offset));

  return header_to_info_.find(header_offset)->second;
}

const BytecodeLivenessState* BytecodeAnalysis::GetInLivenessFor(
    int offset) const {
  if (!do_liveness_analysis_) return nullptr;

  return liveness_map_.GetInLiveness(offset);
}

const BytecodeLivenessState* BytecodeAnalysis::GetOutLivenessFor(
    int offset) const {
  if (!do_liveness_analysis_) return nullptr;

  return liveness_map_.GetOutLiveness(offset);
}

std::ostream& BytecodeAnalysis::PrintLivenessTo(std::ostream& os) const {
  interpreter::BytecodeArrayIterator iterator(bytecode_array());

  for (; !iterator.done(); iterator.Advance()) {
    int current_offset = iterator.current_offset();

    const BitVector& in_liveness =
        GetInLivenessFor(current_offset)->bit_vector();
    const BitVector& out_liveness =
        GetOutLivenessFor(current_offset)->bit_vector();

    for (int i = 0; i < in_liveness.length(); ++i) {
      os << (in_liveness.Contains(i) ? "L" : ".");
    }
    os << " -> ";

    for (int i = 0; i < out_liveness.length(); ++i) {
      os << (out_liveness.Contains(i) ? "L" : ".");
    }

    os << " | " << current_offset << ": ";
    iterator.PrintTo(os) << std::endl;
  }

  return os;
}

#if DEBUG
bool BytecodeAnalysis::ResumeJumpTargetsAreValid() {
  bool valid = true;

  // Find the generator switch.
  interpreter::BytecodeArrayRandomIterator iterator(bytecode_array(), zone());
  for (iterator.GoToStart(); iterator.IsValid(); ++iterator) {
    if (iterator.current_bytecode() == Bytecode::kSwitchOnGeneratorState) {
      break;
    }
  }

  // If the iterator is invalid, we've reached the end without finding the
  // generator switch. Similarly, if we are OSR-ing, we're not resuming, so we
  // need no jump targets. So, ensure there are no jump targets and exit.
  if (!iterator.IsValid() || HasOsrEntryPoint()) {
    // Check top-level.
    if (!resume_jump_targets().empty()) {
      PrintF(stderr,
             "Found %zu top-level resume targets but no resume switch\n",
             resume_jump_targets().size());
      valid = false;
    }
    // Check loops.
    for (const std::pair<const int, LoopInfo>& loop_info : header_to_info_) {
      if (!loop_info.second.resume_jump_targets().empty()) {
        PrintF(stderr,
               "Found %zu resume targets at loop at offset %d, but no resume "
               "switch\n",
               loop_info.second.resume_jump_targets().size(), loop_info.first);
        valid = false;
      }
    }

    return valid;
  }

  // Otherwise, we've found the resume switch. Check that the top level jumps
  // only to leaves and loop headers, then check that each loop header handles
  // all the unresolved jumps, also jumping only to leaves and inner loop
  // headers.

  // First collect all required suspend ids.
  std::map<int, int> unresolved_suspend_ids;
  for (const interpreter::JumpTableTargetOffset& offset :
       iterator.GetJumpTableTargetOffsets()) {
    int suspend_id = offset.case_value;
    int resume_offset = offset.target_offset;

    unresolved_suspend_ids[suspend_id] = resume_offset;
  }

  // Check top-level.
  if (!ResumeJumpTargetLeavesResolveSuspendIds(-1, resume_jump_targets(),
                                               &unresolved_suspend_ids)) {
    valid = false;
  }
  // Check loops.
  for (const std::pair<const int, LoopInfo>& loop_info : header_to_info_) {
    if (!ResumeJumpTargetLeavesResolveSuspendIds(
            loop_info.first, loop_info.second.resume_jump_targets(),
            &unresolved_suspend_ids)) {
      valid = false;
    }
  }

  // Check that everything is resolved.
  if (!unresolved_suspend_ids.empty()) {
    PrintF(stderr,
           "Found suspend ids that are not resolved by a final leaf resume "
           "jump:\n");

    for (const std::pair<const int, int>& target : unresolved_suspend_ids) {
      PrintF(stderr, "  %d -> %d\n", target.first, target.second);
    }
    valid = false;
  }

  return valid;
}

bool BytecodeAnalysis::ResumeJumpTargetLeavesResolveSuspendIds(
    int parent_offset, const ZoneVector<ResumeJumpTarget>& resume_jump_targets,
    std::map<int, int>* unresolved_suspend_ids) {
  bool valid = true;
  for (const ResumeJumpTarget& target : resume_jump_targets) {
    std::map<int, int>::iterator it =
        unresolved_suspend_ids->find(target.suspend_id());
    if (it == unresolved_suspend_ids->end()) {
      PrintF(
          stderr,
          "No unresolved suspend found for resume target with suspend id %d\n",
          target.suspend_id());
      valid = false;
      continue;
    }
    int expected_target = it->second;

    if (target.is_leaf()) {
      // Leaves should have the expected target as their target.
      if (target.target_offset() != expected_target) {
        PrintF(
            stderr,
            "Expected leaf resume target for id %d to have target offset %d, "
            "but had %d\n",
            target.suspend_id(), expected_target, target.target_offset());
        valid = false;
      } else {
        // Make sure we're resuming to a Resume bytecode
        interpreter::BytecodeArrayAccessor assessor(bytecode_array(),
                                                    target.target_offset());
        if (assessor.current_bytecode() != Bytecode::kResumeGenerator) {
          PrintF(stderr,
                 "Expected resume target for id %d, offset %d, to be "
                 "ResumeGenerator, but found %s\n",
                 target.suspend_id(), target.target_offset(),
                 Bytecodes::ToString(assessor.current_bytecode()));

          valid = false;
        }
      }
      // We've resolved this suspend id, so erase it to make sure we don't
      // resolve it twice.
      unresolved_suspend_ids->erase(it);
    } else {
      // Non-leaves should have a direct inner loop header as their target.
      if (!IsLoopHeader(target.target_offset())) {
        PrintF(stderr,
               "Expected non-leaf resume target for id %d to have a loop "
               "header at target offset %d\n",
               target.suspend_id(), target.target_offset());
        valid = false;
      } else {
        LoopInfo loop_info = GetLoopInfoFor(target.target_offset());
        if (loop_info.parent_offset() != parent_offset) {
          PrintF(stderr,
                 "Expected non-leaf resume target for id %d to have a direct "
                 "inner loop at target offset %d\n",
                 target.suspend_id(), target.target_offset());
          valid = false;
        }
        // If the target loop is a valid inner loop, we'll check its validity
        // when we analyze its resume targets.
      }
    }
  }
  return valid;
}

bool BytecodeAnalysis::LivenessIsValid() {
  interpreter::BytecodeArrayRandomIterator iterator(bytecode_array(), zone());

  BytecodeLivenessState previous_liveness(bytecode_array()->register_count(),
                                          zone());

  int invalid_offset = -1;
  int which_invalid = -1;

  BytecodeLivenessState* next_bytecode_in_liveness = nullptr;

  // Ensure that there are no liveness changes if we iterate one more time.
  for (iterator.GoToEnd(); iterator.IsValid(); --iterator) {
    Bytecode bytecode = iterator.current_bytecode();

    int current_offset = iterator.current_offset();

    BytecodeLiveness& liveness = liveness_map_.GetLiveness(current_offset);

    previous_liveness.CopyFrom(*liveness.out);

    UpdateOutLiveness(bytecode, *liveness.out, next_bytecode_in_liveness,
                      iterator, liveness_map_);
    // UpdateOutLiveness skips kJumpLoop, so we update it manually.
    if (bytecode == Bytecode::kJumpLoop) {
      int target_offset = iterator.GetJumpTargetOffset();
      liveness.out->Union(*liveness_map_.GetInLiveness(target_offset));
    }

    if (!liveness.out->Equals(previous_liveness)) {
      // Reset the invalid liveness.
      liveness.out->CopyFrom(previous_liveness);
      invalid_offset = current_offset;
      which_invalid = 1;
      break;
    }

    previous_liveness.CopyFrom(*liveness.in);

    liveness.in->CopyFrom(*liveness.out);
    UpdateInLiveness(bytecode, *liveness.in, iterator);

    if (!liveness.in->Equals(previous_liveness)) {
      // Reset the invalid liveness.
      liveness.in->CopyFrom(previous_liveness);
      invalid_offset = current_offset;
      which_invalid = 0;
      break;
    }

    next_bytecode_in_liveness = liveness.in;
  }

  // Ensure that the accumulator is not live when jumping out of a loop, or on
  // the back-edge of a loop.
  for (iterator.GoToStart(); iterator.IsValid() && invalid_offset == -1;
       ++iterator) {
    Bytecode bytecode = iterator.current_bytecode();
    int current_offset = iterator.current_offset();
    int loop_header = GetLoopOffsetFor(current_offset);

    // We only care if we're inside a loop.
    if (loop_header == -1) continue;

    // We only care about jumps.
    if (!Bytecodes::IsJump(bytecode)) continue;

    int jump_target = iterator.GetJumpTargetOffset();

    // If this is a forward jump to somewhere else in the same loop, ignore it.
    if (Bytecodes::IsForwardJump(bytecode) &&
        GetLoopOffsetFor(jump_target) == loop_header) {
      continue;
    }

    // The accumulator must be dead at the start of the target of the jump.
    if (liveness_map_.GetLiveness(jump_target).in->AccumulatorIsLive()) {
      invalid_offset = jump_target;
      which_invalid = 0;
      break;
    }
  }

  if (invalid_offset != -1) {
    OFStream of(stderr);
    of << "Invalid liveness:" << std::endl;

    // Dump the bytecode, annotated with the liveness and marking loops.

    int loop_indent = 0;

    interpreter::BytecodeArrayIterator forward_iterator(bytecode_array());
    for (; !forward_iterator.done(); forward_iterator.Advance()) {
      int current_offset = forward_iterator.current_offset();
      const BitVector& in_liveness =
          GetInLivenessFor(current_offset)->bit_vector();
      const BitVector& out_liveness =
          GetOutLivenessFor(current_offset)->bit_vector();

      for (int i = 0; i < in_liveness.length(); ++i) {
        of << (in_liveness.Contains(i) ? 'L' : '.');
      }

      of << " | ";

      for (int i = 0; i < out_liveness.length(); ++i) {
        of << (out_liveness.Contains(i) ? 'L' : '.');
      }

      of << " : " << current_offset << " : ";

      // Draw loop back edges by indentin everything between loop headers and
      // jump loop instructions.
      if (forward_iterator.current_bytecode() == Bytecode::kJumpLoop) {
        loop_indent--;
      }
      for (int i = 0; i < loop_indent; ++i) {
        of << "| ";
      }
      if (forward_iterator.current_bytecode() == Bytecode::kJumpLoop) {
        of << "`-";
      } else if (IsLoopHeader(current_offset)) {
        of << ".>";
        loop_indent++;
      }
      forward_iterator.PrintTo(of);
      if (Bytecodes::IsJump(forward_iterator.current_bytecode())) {
        of << " (@" << forward_iterator.GetJumpTargetOffset() << ")";
      }
      of << std::endl;

      if (current_offset == invalid_offset) {
        // Underline the invalid liveness.
        if (which_invalid == 0) {
          for (int i = 0; i < in_liveness.length(); ++i) {
            of << '^';
          }
          for (int i = 0; i < out_liveness.length() + 3; ++i) {
            of << ' ';
          }
        } else {
          for (int i = 0; i < in_liveness.length() + 3; ++i) {
            of << ' ';
          }
          for (int i = 0; i < out_liveness.length(); ++i) {
            of << '^';
          }
        }

        // Make sure to draw the loop indentation marks on this additional line.
        of << " : " << current_offset << " : ";
        for (int i = 0; i < loop_indent; ++i) {
          of << "| ";
        }

        of << std::endl;
      }
    }
  }

  return invalid_offset == -1;
}
#endif

}  // namespace compiler
}  // namespace internal
}  // namespace v8
