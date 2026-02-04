// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-peephole.h"

#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "src/base/lazy-instance.h"
#include "src/flags/flags.h"
#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-generator-inl.h"
#include "src/regexp/regexp-bytecode-generator.h"
#include "src/regexp/regexp-bytecodes-inl.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/utils/memcopy.h"
#include "src/utils/utils.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

namespace {

class BytecodeArgument {
 public:
  BytecodeArgument(int offset, int length) : offset_(offset), length_(length) {}

  int offset() const { return offset_; }
  int length() const { return length_; }

 private:
  // TODO(jgruber): This should store {offset,type} as well.
  // TODO(jgruber): Consider changing offset_ to be relative to the current
  // bytecode instead of the start of the bytecode sequence that is being
  // optimized. It is confusing that src/dst offsets have different semantics.
  int offset_;
  int length_;
};

// Describes a bytecode operand for use in a peephole sequence.
struct OpInfo {
  uint16_t offset;
  RegExpBytecodeOperandType type;
  constexpr int size() const { return RegExpBytecodes::Size(type); }

  // Usage:
  //   OpInfo::Get<RegExpBytecodeOperands<BYTECODE>,
  //   RegExpBytecodeOperands<BYTECODE>::Operand::OPERAND>()
  template <class kBytecodeOperands, auto kOperand>
  static OpInfo Get() {
    static constexpr int kOffset = kBytecodeOperands::Offset(kOperand);
    static constexpr RegExpBytecodeOperandType kType =
        kBytecodeOperands::Type(kOperand);
    DCHECK_LE(static_cast<uint32_t>(kOffset),
              std::numeric_limits<decltype(offset)>::max());
    return {kOffset, kType};
  }
};
static_assert(sizeof(OpInfo) <= kSystemPointerSize);  // Passed by value.

class BytecodeArgumentMapping : public BytecodeArgument {
 public:
  enum class Type : uint8_t { kDefault, kOffsetAfterSequence };

  BytecodeArgumentMapping(int offset, int length, OpInfo op_info)
      : BytecodeArgument(offset, length),
        type_(Type::kDefault),
        op_info_(op_info) {}

  BytecodeArgumentMapping(Type type, OpInfo op_info)
      : BytecodeArgument(-1, -1), type_(type), op_info_(op_info) {
    DCHECK_NE(type, Type::kDefault);
  }

  Type type() const { return type_; }
  int new_offset() const { return op_info_.offset; }
  RegExpBytecodeOperandType new_operand_type() const { return op_info_.type; }
  int new_length() const { return op_info_.size(); }

 private:
  Type type_;
  OpInfo op_info_;
};

struct BytecodeArgumentCheck : public BytecodeArgument {
  enum CheckType { kCheckAddress = 0, kCheckValue };
  CheckType type;
  int check_offset;
  int check_length;

  BytecodeArgumentCheck(int offset, int length, int check_offset)
      : BytecodeArgument(offset, length),
        type(kCheckAddress),
        check_offset(check_offset) {}
  BytecodeArgumentCheck(int offset, int length, int check_offset,
                        int check_length)
      : BytecodeArgument(offset, length),
        type(kCheckValue),
        check_offset(check_offset),
        check_length(check_length) {}
};

// Trie-Node for storing bytecode sequences we want to optimize.
class BytecodeSequenceNode {
 public:
  explicit BytecodeSequenceNode(std::optional<RegExpBytecode> bytecode);
  // Adds a new node as child of the current node if it isn't a child already.
  BytecodeSequenceNode& FollowedBy(RegExpBytecode bytecode);
  // Marks the end of a sequence and sets optimized bytecode to replace all
  // bytecodes of the sequence with.
  BytecodeSequenceNode& ReplaceWith(RegExpBytecode bytecode);
  // Maps arguments of bytecodes in the sequence to the optimized bytecode.
  // Order of invocation determines order of arguments in the optimized
  // bytecode.
  // Invoking this method is only allowed on nodes that mark the end of a valid
  // sequence (i.e. after ReplaceWith()).
  // to_op_info: Operand info of the argument in the optimized bytecode.
  // from_bytecode_sequence_index: Zero-based index of the referred bytecode
  // within the sequence (e.g. the bytecode passed to CreateSequence() has
  // index 0).
  // from_op_info: Operand info of the argument in the referred bytecode.
  BytecodeSequenceNode& MapArgument(OpInfo to_op_info,
                                    int from_bytecode_sequence_index,
                                    OpInfo from_op_info);

  // Emits the offset after the whole sequence.
  // This should be used for every sequence that doesn't end in an unconditional
  // jump. The offset isn't statically known, as bytecodes might be preserved
  // after the sequence if they were jump targets from bytecodes outside the
  // sequence. The emitted offset is after these potentially preserved
  // bytecodes.
  BytecodeSequenceNode& EmitOffsetAfterSequence(OpInfo op_info);
  // Verifies that we've created mappings in the order they are specified.
  bool BytecodeArgumentMappingCreatedInOrder(OpInfo op_info);
  // Adds a check to the sequence node making it only a valid sequence when the
  // argument of the current bytecode at the specified offset matches the offset
  // to check against.
  // op_info: Operand info of the argument to check.
  // check_byte_offset: Zero-based offset relative to the beginning of the
  // sequence that needs to match the value given by argument_offset. (e.g.
  // check_byte_offset 0 matches the address of the first bytecode in the
  // sequence).
  BytecodeSequenceNode& IfArgumentEqualsOffset(OpInfo op_info,
                                               int check_byte_offset);

  // Adds a check to the sequence node making it only a valid sequence when the
  // argument of the current bytecode at the specified offset matches the
  // argument of another bytecode in the sequence.
  // This is similar to IfArgumentEqualsOffset, except that this method matches
  // the values of both arguments.
  BytecodeSequenceNode& IfArgumentEqualsValueAtOffset(
      OpInfo this_op_info, int other_bytecode_index_in_sequence,
      OpInfo other_op_info);

  // Marks an argument as unused.
  // All arguments that are not mapped explicitly have to be marked as unused.
  // bytecode_index_in_sequence: Zero-based index of the referred bytecode
  // within the sequence (e.g. the bytecode passed to CreateSequence() has
  // index 0).
  // op_info: Operand info of the argument to ignore.
  BytecodeSequenceNode& IgnoreArgument(int bytecode_index_in_sequence,
                                       OpInfo op_info);
  // Checks if the current node is valid for the sequence. I.e. all conditions
  // set by IfArgumentEqualsOffset and IfArgumentEquals are fulfilled by this
  // node for the actual bytecode sequence.
  bool CheckArguments(const uint8_t* bytecode, int pc) const;
  // Returns whether this node marks the end of a valid sequence (i.e. can be
  // replaced with an optimized bytecode).
  bool IsSequence() const;
  // Returns the length of the sequence in bytes.
  int SequenceLength() const;
  // Returns the optimized bytecode for the node.
  RegExpBytecode OptimizedBytecode() const;
  // Returns the child of the current node matching the given bytecode or
  // nullptr if no such child is found.
  BytecodeSequenceNode* Find(RegExpBytecode bytecode) const;
  // Returns number of arguments mapped to the current node.
  // Invoking this method is only allowed on nodes that mark the end of a valid
  // sequence (i.e. if IsSequence())
  size_t ArgumentSize() const;
  // Returns the argument-mapping of the argument at index.
  // Invoking this method is only allowed on nodes that mark the end of a valid
  // sequence (i.e. if IsSequence())
  BytecodeArgumentMapping ArgumentMapping(size_t index) const;
  // Returns an iterator to begin of ignored arguments.
  // Invoking this method is only allowed on nodes that mark the end of a valid
  // sequence (i.e. if IsSequence())
  std::vector<BytecodeArgument>::const_iterator ArgumentIgnoredBegin() const;
  // Returns an iterator to end of ignored arguments.
  // Invoking this method is only allowed on nodes that mark the end of a valid
  // sequence (i.e. if IsSequence())
  std::vector<BytecodeArgument>::const_iterator ArgumentIgnoredEnd() const;
  // Returns whether the current node has ignored argument or not.
  bool HasIgnoredArguments() const;

 private:
  // Returns a node in the sequence specified by its index within the sequence.
  BytecodeSequenceNode& GetNodeByIndexInSequence(int index_in_sequence);

  std::optional<RegExpBytecode> bytecode_;
  std::optional<RegExpBytecode> bytecode_replacement_;
  int index_in_sequence_;
  int start_offset_;
  BytecodeSequenceNode* parent_;
  std::unordered_map<RegExpBytecode, std::unique_ptr<BytecodeSequenceNode>>
      children_;
  std::vector<BytecodeArgumentMapping> argument_mapping_;
  std::vector<BytecodeArgumentCheck> argument_check_;
  std::vector<BytecodeArgument> argument_ignored_;
};

class RegExpBytecodePeepholeSequences {
 public:
  RegExpBytecodePeepholeSequences();
  const BytecodeSequenceNode* sequences() const { return &sequences_; }

 private:
  void DefineStandardSequences();
  BytecodeSequenceNode& CreateSequence(RegExpBytecode bytecode);

  BytecodeSequenceNode sequences_;
};

class RegExpBytecodePeephole {
 public:
  // Parses bytecode and fills the internal buffer with the potentially
  // optimized bytecode. Returns true when optimizations were performed, false
  // otherwise.
  static bool OptimizeBytecode(Zone* zone,
                               const RegExpBytecodeWriter* src_writer,
                               RegExpBytecodeWriter* dst_writer);

 private:
  RegExpBytecodePeephole(Zone* zone, const RegExpBytecodeWriter* src_writer,
                         RegExpBytecodeWriter* dst_writer);

  // Checks for optimization candidates at pc and emits optimized bytecode to
  // the internal buffer. Returns the length of replaced bytecodes in bytes.
  int TryOptimizeSequence(const uint8_t* bytecode, int bytecode_length,
                          int start_pc);
  // Emits optimized bytecode to the internal buffer. start_pc points to the
  // start of the sequence in bytecode and last_node is the last
  // BytecodeSequenceNode of the matching sequence found.
  void EmitOptimization(int start_pc, const uint8_t* bytecode,
                        const BytecodeSequenceNode& last_node);
  // Adds a relative jump destination fixup at pos.
  // Jump destination fixups are used to find offsets in the new bytecode that
  // can be jumped to.
  void AddJumpDestinationFixup(int fixup, int pos);
  // Sets an absolute jump destination fixup at pos.
  void SetJumpDestinationFixup(int fixup, int pos);
  // Updates all jump targets in the new bytecode.
  void FixJumps();
  void EmitArgument(int start_pc, const uint8_t* bytecode,
                    BytecodeArgumentMapping arg);
  int pc() const;
  Zone* zone() const;

  RegExpBytecodeWriter* const dst_writer_;
  const RegExpBytecodeWriter* const src_writer_;

  // TODO(jgruber): We should also replace all of these raw offsets with
  // OpInfo. That should allow us to not expose the "raw" Emit publicly in the
  // Writer.
  // Number of times a jump destination is used within the bytecode.
  // Key: Jump destination (offset in old bytecode).
  // Value: Number of times jump destination is used.
  ZoneMap<int, int> jump_usage_counts_;
  // Maps offsets in old bytecode to fixups of destinations (delta to new
  // bytecode).
  // Key: Offset in old bytecode from where the fixup is valid.
  // Value: Delta to map jump destinations from old bytecode to new bytecode in
  // bytes.
  ZoneMap<int, int> jump_destination_fixups_;

  Zone* const zone_;

  // Points at the first pc in src_writer that has not yet been emitted. Used
  // for batch copying unchanged regions of the incoming bytecodes.
  int next_src_pc_to_emit_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpBytecodePeephole);
};

template <typename T>
T GetValue(const uint8_t* buffer, int pos) {
  DCHECK(IsAligned(reinterpret_cast<Address>(buffer + pos), alignof(T)));
  return *reinterpret_cast<const T*>(buffer + pos);
}

int32_t GetArgumentValue(const uint8_t* bytecode, int offset, int length) {
  switch (length) {
    case 1:
      return GetValue<uint8_t>(bytecode, offset);
    case 2:
      return GetValue<int16_t>(bytecode, offset);
    case 4:
      return GetValue<int32_t>(bytecode, offset);
    default:
      UNREACHABLE();
  }
}

BytecodeSequenceNode::BytecodeSequenceNode(
    std::optional<RegExpBytecode> bytecode)
    : bytecode_(bytecode),
      bytecode_replacement_(std::nullopt),
      index_in_sequence_(0),
      start_offset_(0),
      parent_(nullptr) {}

BytecodeSequenceNode& BytecodeSequenceNode::FollowedBy(
    RegExpBytecode bytecode) {
  if (children_.find(bytecode) == children_.end()) {
    auto new_node = std::make_unique<BytecodeSequenceNode>(bytecode);
    // If node is not the first in the sequence, set offsets and parent.
    if (bytecode_.has_value()) {
      new_node->start_offset_ =
          start_offset_ + RegExpBytecodes::Size(bytecode_.value());
      new_node->index_in_sequence_ = index_in_sequence_ + 1;
      new_node->parent_ = this;
    }
    children_[bytecode] = std::move(new_node);
  }

  return *children_[bytecode];
}

BytecodeSequenceNode& BytecodeSequenceNode::ReplaceWith(
    RegExpBytecode bytecode) {
  bytecode_replacement_ = bytecode;

  return *this;
}

BytecodeSequenceNode& BytecodeSequenceNode::MapArgument(
    OpInfo to_op_info, int from_bytecode_sequence_index, OpInfo from_op_info) {
  int src_offset = from_op_info.offset;
  int src_size = from_op_info.size();

  DCHECK_LE(from_bytecode_sequence_index, index_in_sequence_);
  DCHECK(BytecodeArgumentMappingCreatedInOrder(to_op_info));

  BytecodeSequenceNode& ref_node =
      GetNodeByIndexInSequence(from_bytecode_sequence_index);
  DCHECK_LT(src_offset, RegExpBytecodes::Size(ref_node.bytecode_.value()));

  int offset_from_start_of_sequence = ref_node.start_offset_ + src_offset;
  argument_mapping_.push_back(BytecodeArgumentMapping{
      offset_from_start_of_sequence, src_size, to_op_info});
  return *this;
}

BytecodeSequenceNode& BytecodeSequenceNode::EmitOffsetAfterSequence(
    OpInfo op_info) {
  DCHECK(BytecodeArgumentMappingCreatedInOrder(op_info));
  argument_mapping_.push_back(BytecodeArgumentMapping{
      BytecodeArgumentMapping::Type::kOffsetAfterSequence, op_info});
  return *this;
}

bool BytecodeSequenceNode::BytecodeArgumentMappingCreatedInOrder(
    OpInfo op_info) {
  DCHECK(IsSequence());
  if (argument_mapping_.empty()) return true;

  const BytecodeArgumentMapping& m = argument_mapping_.back();
  int offset_after_last = m.new_offset() + m.new_length();
  // TODO(jgruber): It'd be more precise to distinguish between special and
  // basic operand types, but we currently don't expose that information
  // except through templates.
  int dst_size = op_info.size();
  int alignment = std::min(dst_size, kBytecodeAlignment);
  return RoundUp(offset_after_last, alignment) == op_info.offset;
}

BytecodeSequenceNode& BytecodeSequenceNode::IfArgumentEqualsOffset(
    OpInfo op_info, int check_byte_offset) {
  int size = op_info.size();
  int offset = op_info.offset;

  DCHECK_LT(offset, RegExpBytecodes::Size(bytecode_.value()));
  DCHECK(size == 1 || size == 2 || size == 4);

  int offset_from_start_of_sequence = start_offset_ + offset;

  argument_check_.push_back(BytecodeArgumentCheck{offset_from_start_of_sequence,
                                                  size, check_byte_offset});

  return *this;
}

BytecodeSequenceNode& BytecodeSequenceNode::IfArgumentEqualsValueAtOffset(
    OpInfo this_op_info, int other_bytecode_index_in_sequence,
    OpInfo other_op_info) {
  int size_1 = this_op_info.size();
  int size_2 = other_op_info.size();

  DCHECK_LT(this_op_info.offset, RegExpBytecodes::Size(bytecode_.value()));
  DCHECK_LE(other_bytecode_index_in_sequence, index_in_sequence_);
  DCHECK_EQ(size_1, size_2);

  BytecodeSequenceNode& ref_node =
      GetNodeByIndexInSequence(other_bytecode_index_in_sequence);
  DCHECK_LT(other_op_info.offset,
            RegExpBytecodes::Size(ref_node.bytecode_.value()));

  int offset_from_start_of_sequence = start_offset_ + this_op_info.offset;
  int other_offset_from_start_of_sequence =
      ref_node.start_offset_ + other_op_info.offset;

  argument_check_.push_back(
      BytecodeArgumentCheck{offset_from_start_of_sequence, size_1,
                            other_offset_from_start_of_sequence, size_2});

  return *this;
}

BytecodeSequenceNode& BytecodeSequenceNode::IgnoreArgument(
    int bytecode_index_in_sequence, OpInfo op_info) {
  int size = op_info.size();
  int offset = op_info.offset;

  DCHECK(IsSequence());
  DCHECK_LE(bytecode_index_in_sequence, index_in_sequence_);

  BytecodeSequenceNode& ref_node =
      GetNodeByIndexInSequence(bytecode_index_in_sequence);
  DCHECK_LT(offset, RegExpBytecodes::Size(ref_node.bytecode_.value()));

  int offset_from_start_of_sequence = ref_node.start_offset_ + offset;

  argument_ignored_.push_back(
      BytecodeArgument{offset_from_start_of_sequence, size});

  return *this;
}

bool BytecodeSequenceNode::CheckArguments(const uint8_t* bytecode,
                                          int pc) const {
  bool is_valid = true;
  for (auto check_iter = argument_check_.begin();
       check_iter != argument_check_.end() && is_valid; check_iter++) {
    auto value = GetArgumentValue(bytecode, pc + check_iter->offset(),
                                  check_iter->length());
    if (check_iter->type == BytecodeArgumentCheck::kCheckAddress) {
      is_valid &= value == pc + check_iter->check_offset;
    } else if (check_iter->type == BytecodeArgumentCheck::kCheckValue) {
      auto other_value = GetArgumentValue(
          bytecode, pc + check_iter->check_offset, check_iter->check_length);
      is_valid &= value == other_value;
    } else {
      UNREACHABLE();
    }
  }
  return is_valid;
}

bool BytecodeSequenceNode::IsSequence() const {
  return bytecode_replacement_.has_value();
}

int BytecodeSequenceNode::SequenceLength() const {
  return start_offset_ + RegExpBytecodes::Size(bytecode_.value());
}

RegExpBytecode BytecodeSequenceNode::OptimizedBytecode() const {
  return bytecode_replacement_.value();
}

BytecodeSequenceNode* BytecodeSequenceNode::Find(
    RegExpBytecode bytecode) const {
  auto found = children_.find(bytecode);
  if (found == children_.end()) return nullptr;
  return found->second.get();
}

size_t BytecodeSequenceNode::ArgumentSize() const {
  DCHECK(IsSequence());
  return argument_mapping_.size();
}

BytecodeArgumentMapping BytecodeSequenceNode::ArgumentMapping(
    size_t index) const {
  DCHECK(IsSequence());
  DCHECK_LT(index, argument_mapping_.size());

  return argument_mapping_.at(index);
}

std::vector<BytecodeArgument>::const_iterator
BytecodeSequenceNode::ArgumentIgnoredBegin() const {
  DCHECK(IsSequence());
  return argument_ignored_.begin();
}

std::vector<BytecodeArgument>::const_iterator
BytecodeSequenceNode::ArgumentIgnoredEnd() const {
  DCHECK(IsSequence());
  return argument_ignored_.end();
}

bool BytecodeSequenceNode::HasIgnoredArguments() const {
  return !argument_ignored_.empty();
}

BytecodeSequenceNode& BytecodeSequenceNode::GetNodeByIndexInSequence(
    int index_in_sequence) {
  DCHECK_LE(index_in_sequence, index_in_sequence_);

  if (index_in_sequence < index_in_sequence_) {
    DCHECK(parent_ != nullptr);
    return parent_->GetNodeByIndexInSequence(index_in_sequence);
  } else {
    return *this;
  }
}

RegExpBytecodePeepholeSequences::RegExpBytecodePeepholeSequences()
    : sequences_(std::nullopt) {
  DefineStandardSequences();
}

BytecodeSequenceNode& RegExpBytecodePeepholeSequences::CreateSequence(
    RegExpBytecode bytecode) {
  return sequences_.FollowedBy(bytecode);
}

RegExpBytecodePeephole::RegExpBytecodePeephole(
    Zone* zone, const RegExpBytecodeWriter* src_writer,
    RegExpBytecodeWriter* dst_writer)
    : dst_writer_(dst_writer),
      src_writer_(src_writer),
      jump_usage_counts_(zone),
      jump_destination_fixups_(zone),
      zone_(zone),
      next_src_pc_to_emit_(0) {
  dst_writer_->buffer().reserve(src_writer_->length());
  // Prepare jump usage counts.
  for (auto jump_edge : src_writer_->jump_edges()) {
    int jump_destination = jump_edge.second;
    jump_usage_counts_[jump_destination]++;
  }
  // Sentinel fixups at beginning of bytecode (position -1) so we don't have to
  // check for end of iterator inside the fixup loop.
  // In general fixups are deltas of original offsets of jump
  // sources/destinations (in the old bytecode) to find them in the new
  // bytecode. All jump targets are fixed after the new bytecode is fully
  // emitted in the internal buffer.
  jump_destination_fixups_.emplace(-1, 0);
  // Sentinel fixups at end of (old) bytecode so we don't have to check for
  // end of iterator inside the fixup loop.
  DCHECK_LE(src_writer_->length(), std::numeric_limits<int>::max());
  jump_destination_fixups_.emplace(static_cast<int>(src_writer_->length()), 0);
}

void RegExpBytecodePeepholeSequences::DefineStandardSequences() {
  using B = RegExpBytecode;
#define I(BYTECODE, OPERAND)                    \
  OpInfo::Get<RegExpBytecodeOperands<BYTECODE>, \
              RegExpBytecodeOperands<BYTECODE>::Operand::OPERAND>()
#define T(OPERAND) I(Target, OPERAND)

  // Commonly used sequences can be found by creating regexp bytecode traces
  // (--trace-regexp-bytecodes) and using v8/tools/regexp-sequences.py.

  {
    static constexpr auto Target = B::kAdvanceCpAndGoto;
    CreateSequence(B::kAdvanceCurrentPosition)
        .FollowedBy(B::kGoTo)
        .ReplaceWith(Target)
        .MapArgument(T(by), 0, I(B::kAdvanceCurrentPosition, by))
        .MapArgument(T(on_goto), 1, I(B::kGoTo, label));
  }

  {
    static constexpr auto Target = B::kSkipUntilBitInTable;
    CreateSequence(B::kLoadCurrentCharacter)
        .FollowedBy(B::kCheckBitInTable)
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 0, I(B::kLoadCurrentCharacter, cp_offset))
        .MapArgument(T(advance_by), 2, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(table), 1, I(B::kCheckBitInTable, table))
        .MapArgument(T(on_match), 1, I(B::kCheckBitInTable, on_bit_set))
        .MapArgument(T(on_no_match), 0, I(B::kLoadCurrentCharacter, on_failure))
        .IgnoreArgument(2, I(B::kAdvanceCpAndGoto, on_goto));
  }

  {
    static constexpr auto Target = B::kSkipUntilCharPosChecked;
    CreateSequence(B::kCheckPosition)
        .FollowedBy(B::kLoadCurrentCharacterUnchecked)
        .FollowedBy(B::kCheckCharacter)
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 1,
                     I(B::kLoadCurrentCharacterUnchecked, cp_offset))
        .MapArgument(T(advance_by), 3, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(character), 2, I(B::kCheckCharacter, character))
        .MapArgument(T(eats_at_least), 0, I(B::kCheckPosition, cp_offset))
        .MapArgument(T(on_match), 2, I(B::kCheckCharacter, on_equal))
        .MapArgument(T(on_no_match), 0, I(B::kCheckPosition, on_failure))
        .IgnoreArgument(3, I(B::kAdvanceCpAndGoto, on_goto));
  }

  {
    static constexpr auto Target = B::kSkipUntilCharAnd;
    CreateSequence(B::kCheckPosition)
        .FollowedBy(B::kLoadCurrentCharacterUnchecked)
        .FollowedBy(B::kCheckCharacterAfterAnd)
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 1,
                     I(B::kLoadCurrentCharacterUnchecked, cp_offset))
        .MapArgument(T(advance_by), 3, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(character), 2, I(B::kCheckCharacterAfterAnd, character))
        .MapArgument(T(mask), 2, I(B::kCheckCharacterAfterAnd, mask))
        .MapArgument(T(eats_at_least), 0, I(B::kCheckPosition, cp_offset))
        .MapArgument(T(on_match), 2, I(B::kCheckCharacterAfterAnd, on_equal))
        .MapArgument(T(on_no_match), 0, I(B::kCheckPosition, on_failure))
        .IgnoreArgument(3, I(B::kAdvanceCpAndGoto, on_goto));
  }

  // TODO(pthier): It might make sense for short sequences like this one to only
  // optimize them if the resulting optimization is not longer than the current
  // one. This could be the case if there are jumps inside the sequence and we
  // have to replicate parts of the sequence. A method to mark such sequences
  // might be useful.
  {
    static constexpr auto Target = B::kSkipUntilChar;
    CreateSequence(B::kLoadCurrentCharacter)
        .FollowedBy(B::kCheckCharacter)
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 0, I(B::kLoadCurrentCharacter, cp_offset))
        .MapArgument(T(advance_by), 2, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(character), 1, I(B::kCheckCharacter, character))
        .MapArgument(T(on_match), 1, I(B::kCheckCharacter, on_equal))
        .MapArgument(T(on_no_match), 0, I(B::kLoadCurrentCharacter, on_failure))
        .IgnoreArgument(2, I(B::kAdvanceCpAndGoto, on_goto));
  }

  {
    static constexpr auto Target = B::kSkipUntilCharOrChar;
    CreateSequence(B::kLoadCurrentCharacter)
        .FollowedBy(B::kCheckCharacter)
        .FollowedBy(B::kCheckCharacter)
        .IfArgumentEqualsValueAtOffset(I(B::kCheckCharacter, on_equal), 1,
                                       I(B::kCheckCharacter, on_equal))
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 0, I(B::kLoadCurrentCharacter, cp_offset))
        .MapArgument(T(advance_by), 3, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(char1), 1, I(B::kCheckCharacter, character))
        .MapArgument(T(char2), 2, I(B::kCheckCharacter, character))
        .MapArgument(T(on_match), 1, I(B::kCheckCharacter, on_equal))
        .MapArgument(T(on_no_match), 0, I(B::kLoadCurrentCharacter, on_failure))
        .IgnoreArgument(2, I(B::kCheckCharacter, on_equal))
        .IgnoreArgument(3, I(B::kAdvanceCpAndGoto, on_goto));
  }

  {
    static constexpr auto Target = B::kSkipUntilGtOrNotBitInTable;
    CreateSequence(B::kLoadCurrentCharacter)
        .FollowedBy(B::kCheckCharacterGT)
        // Sequence is only valid if the jump target of kCheckCharacterGT is the
        // first bytecode AFTER the whole sequence.
        .IfArgumentEqualsOffset(I(B::kCheckCharacterGT, on_greater), 56)
        .FollowedBy(B::kCheckBitInTable)
        // Sequence is only valid if the jump target of kCheckBitInTable is
        // the kAdvanceCpAndGoto bytecode at the end of the sequence.
        .IfArgumentEqualsOffset(I(B::kCheckBitInTable, on_bit_set), 48)
        .FollowedBy(B::kGoTo)
        // Sequence is only valid if the jump target of kGoTo is the same as the
        // jump target of kCheckCharacterGT (i.e. both jump to the first
        // bytecode AFTER the whole sequence.
        .IfArgumentEqualsValueAtOffset(I(B::kGoTo, label), 1,
                                       I(B::kCheckCharacterGT, on_greater))
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 0, I(B::kLoadCurrentCharacter, cp_offset))
        .MapArgument(T(advance_by), 4, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(character), 1, I(B::kCheckCharacterGT, limit))
        .MapArgument(T(table), 2, I(B::kCheckBitInTable, table))
        .MapArgument(T(on_match), 1, I(B::kCheckCharacterGT, on_greater))
        .MapArgument(T(on_no_match), 0, I(B::kLoadCurrentCharacter, on_failure))
        .IgnoreArgument(2, I(B::kCheckBitInTable, on_bit_set))
        .IgnoreArgument(3, I(B::kGoTo, label))
        .IgnoreArgument(4, I(B::kAdvanceCpAndGoto, on_goto));
  }
  {
    static constexpr auto Target = B::kSkipUntilOneOfMasked;
    CreateSequence(B::kCheckPosition)
        .FollowedBy(B::kLoad4CurrentCharsUnchecked)
        .FollowedBy(B::kAndCheck4Chars)
        // Jump target is the offset of the next AndCheck4Chars (right after
        // AdvanceCpAndGoto).
        .IfArgumentEqualsOffset(I(B::kAndCheck4Chars, on_equal), 0x24)
        .FollowedBy(B::kAdvanceCpAndGoto)
        .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto), 0)
        .FollowedBy(B::kAndCheck4Chars)
        .FollowedBy(B::kAndCheckNot4Chars)
        // Jump target is AdvanceCpAndGoto.
        .IfArgumentEqualsOffset(I(B::kAndCheckNot4Chars, on_not_equal), 0x1c)
        .ReplaceWith(Target)
        .MapArgument(T(cp_offset), 1,
                     I(B::kLoad4CurrentCharsUnchecked, cp_offset))
        .MapArgument(T(advance_by), 3, I(B::kAdvanceCpAndGoto, by))
        .MapArgument(T(both_chars), 2, I(B::kAndCheck4Chars, characters))
        .MapArgument(T(both_mask), 2, I(B::kAndCheck4Chars, mask))
        .MapArgument(T(max_offset), 0, I(B::kCheckPosition, cp_offset))
        .MapArgument(T(chars1), 4, I(B::kAndCheck4Chars, characters))
        .MapArgument(T(mask1), 4, I(B::kAndCheck4Chars, mask))
        .MapArgument(T(chars2), 5, I(B::kAndCheckNot4Chars, characters))
        .MapArgument(T(mask2), 5, I(B::kAndCheckNot4Chars, mask))
        .MapArgument(T(on_match1), 4, I(B::kAndCheck4Chars, on_equal))
        .EmitOffsetAfterSequence(T(on_match2))
        .MapArgument(T(on_failure), 0, I(B::kCheckPosition, on_failure))
        .IgnoreArgument(3, I(B::kAdvanceCpAndGoto, on_goto))
        .IgnoreArgument(2, I(B::kAndCheck4Chars, on_equal));
  }
  // The original bytecode sequence for kSkipUntilOneOfMasked3 is:
  //
  // sequence offset name
  // bc0   0  SkipUntilBitInTable
  // bc1  20  CheckPosition
  // bc2  28  Load4CurrentCharsUnchecked
  // bc3  2c  AndCheck4Chars
  // bc4  3c  AdvanceCpAndGoto
  // bc5  48  Load4CurrentChars
  // bc6  4c  AndCheck4Chars
  // bc7  5c  AndCheck4Chars
  // bc8  6c  AndCheckNot4Chars
  {
    static constexpr int kOffsetOfBc0SkipUntilBitInTable = 0x0;
    static constexpr int kOffsetOfBc1CheckCurrentPosition = 0x20;
    static constexpr int kOffsetOfBc4AdvanceBcAndGoto = 0x3c;
    static constexpr auto Target = B::kSkipUntilOneOfMasked3;
    BytecodeSequenceNode& s0 =
        CreateSequence(B::kSkipUntilBitInTable)
            .IfArgumentEqualsOffset(I(B::kSkipUntilBitInTable, on_no_match),
                                    kOffsetOfBc1CheckCurrentPosition)
            .IfArgumentEqualsOffset(I(B::kSkipUntilBitInTable, on_no_match),
                                    kOffsetOfBc1CheckCurrentPosition);

    DCHECK_EQ(s0.SequenceLength(), 0x20);
    DCHECK_EQ(s0.SequenceLength(), kOffsetOfBc1CheckCurrentPosition);
    static constexpr int kOffsetOfBc5Load4CurrentChars = 0x44;
    BytecodeSequenceNode& s1 =
        s0.FollowedBy(B::kCheckPosition)
            .FollowedBy(B::kLoad4CurrentCharsUnchecked)
            .FollowedBy(B::kAndCheck4Chars)
            .IfArgumentEqualsOffset(I(B::kAndCheck4Chars, on_equal),
                                    kOffsetOfBc5Load4CurrentChars);

    DCHECK_EQ(s1.SequenceLength(), 0x3c);
    DCHECK_EQ(s1.SequenceLength(), kOffsetOfBc4AdvanceBcAndGoto);
    BytecodeSequenceNode& s2 =
        s1.FollowedBy(B::kAdvanceCpAndGoto)
            .IfArgumentEqualsOffset(I(B::kAdvanceCpAndGoto, on_goto),
                                    kOffsetOfBc0SkipUntilBitInTable);

    DCHECK_EQ(s2.SequenceLength(), 0x44);
    DCHECK_EQ(s2.SequenceLength(), kOffsetOfBc5Load4CurrentChars);
    BytecodeSequenceNode& s3 =
        s2.FollowedBy(B::kLoad4CurrentChars)
            .IfArgumentEqualsOffset(I(B::kLoad4CurrentChars, on_failure),
                                    kOffsetOfBc4AdvanceBcAndGoto)
            .FollowedBy(B::kAndCheck4Chars)
            .FollowedBy(B::kAndCheck4Chars)
            .FollowedBy(B::kAndCheckNot4Chars)
            .IfArgumentEqualsOffset(I(B::kAndCheckNot4Chars, on_not_equal),
                                    kOffsetOfBc4AdvanceBcAndGoto);

    s3.ReplaceWith(Target)
        .MapArgument(T(bc0_cp_offset), 0, I(B::kSkipUntilBitInTable, cp_offset))
        .MapArgument(T(bc0_advance_by), 0,
                     I(B::kSkipUntilBitInTable, advance_by))
        .MapArgument(T(bc0_table), 0, I(B::kSkipUntilBitInTable, table))
        .IgnoreArgument(0, I(B::kSkipUntilBitInTable, on_match))
        .IgnoreArgument(0, I(B::kSkipUntilBitInTable, on_no_match))
        .MapArgument(T(bc1_cp_offset), 1, I(B::kCheckPosition, cp_offset))
        .MapArgument(T(bc1_on_failure), 1, I(B::kCheckPosition, on_failure))
        .MapArgument(T(bc2_cp_offset), 2,
                     I(B::kLoad4CurrentCharsUnchecked, cp_offset))
        .MapArgument(T(bc3_characters), 3, I(B::kAndCheck4Chars, characters))
        .MapArgument(T(bc3_mask), 3, I(B::kAndCheck4Chars, mask))
        .IgnoreArgument(3, I(B::kAndCheck4Chars, on_equal))
        .MapArgument(T(bc4_by), 4, I(B::kAdvanceCpAndGoto, by))
        .IgnoreArgument(4, I(B::kAdvanceCpAndGoto, on_goto))
        .MapArgument(T(bc5_cp_offset), 5, I(B::kLoad4CurrentChars, cp_offset))
        .IgnoreArgument(5, I(B::kLoad4CurrentChars, on_failure))
        .MapArgument(T(bc6_characters), 6, I(B::kAndCheck4Chars, characters))
        .MapArgument(T(bc6_mask), 6, I(B::kAndCheck4Chars, mask))
        .MapArgument(T(bc6_on_equal), 6, I(B::kAndCheck4Chars, on_equal))
        .MapArgument(T(bc7_characters), 7, I(B::kAndCheck4Chars, characters))
        .MapArgument(T(bc7_mask), 7, I(B::kAndCheck4Chars, mask))
        .MapArgument(T(bc7_on_equal), 7, I(B::kAndCheck4Chars, on_equal))
        .MapArgument(T(bc8_characters), 8, I(B::kAndCheckNot4Chars, characters))
        .MapArgument(T(bc8_mask), 8, I(B::kAndCheckNot4Chars, mask))
        .IgnoreArgument(8, I(B::kAndCheckNot4Chars, on_not_equal))
        .EmitOffsetAfterSequence(T(fallthrough_jump_target));
  }

#undef I
#undef T
}
bool RegExpBytecodePeephole::OptimizeBytecode(
    Zone* zone, const RegExpBytecodeWriter* src_writer,
    RegExpBytecodeWriter* dst_writer) {
  RegExpBytecodePeephole p(zone, src_writer, dst_writer);

  const uint8_t* bytecode = src_writer->buffer().data();
  int length = src_writer->length();

  int old_pc = 0;
  bool did_optimize = false;

  while (old_pc < length) {
    int replaced_len = p.TryOptimizeSequence(bytecode, length, old_pc);
    if (replaced_len > 0) {
      old_pc += replaced_len;
      did_optimize = true;
    } else {
      int bc_len = RegExpBytecodes::Size(bytecode[old_pc]);
      old_pc += bc_len;
    }
  }

  if (did_optimize) {
    // If we optimized anything, we must flush the remaining unoptimized bytes.
    // If we didn't optimize anything, we leave the dst_writer empty and the
    // caller will continue using src_writer (effectively a no-op pass).
    if (old_pc > p.next_src_pc_to_emit_) {
      dst_writer->EmitRawBytecodeStream(src_writer, p.next_src_pc_to_emit_,
                                        old_pc - p.next_src_pc_to_emit_);
    }
    p.FixJumps();
  }

  return did_optimize;
}

DEFINE_LAZY_LEAKY_OBJECT_GETTER(RegExpBytecodePeepholeSequences,
                                GetStandardSequences)

int RegExpBytecodePeephole::TryOptimizeSequence(const uint8_t* bytecode,
                                                int bytecode_length,
                                                int start_pc) {
  const BytecodeSequenceNode* seq_node = GetStandardSequences()->sequences();
  const BytecodeSequenceNode* valid_seq_end = nullptr;

  int current_pc = start_pc;

  // Check for the longest valid sequence matching any of the pre-defined
  // sequences in the Trie data structure.
  while (current_pc < bytecode_length) {
    seq_node = seq_node->Find(RegExpBytecodes::FromByte(bytecode[current_pc]));
    if (seq_node == nullptr) break;
    if (!seq_node->CheckArguments(bytecode, start_pc)) break;

    if (seq_node->IsSequence()) valid_seq_end = seq_node;
    current_pc += RegExpBytecodes::Size(bytecode[current_pc]);
  }

  if (valid_seq_end) {
    EmitOptimization(start_pc, bytecode, *valid_seq_end);
    return valid_seq_end->SequenceLength();
  }

  return 0;
}

void RegExpBytecodePeephole::EmitOptimization(
    int start_pc, const uint8_t* bytecode,
    const BytecodeSequenceNode& last_node) {
  // Flush any sequence of bytecodes which we haven't emitted yet.
  if (start_pc > next_src_pc_to_emit_) {
    dst_writer_->EmitRawBytecodeStream(src_writer_, next_src_pc_to_emit_,
                                       start_pc - next_src_pc_to_emit_);
  }
  const int sequence_length = last_node.SequenceLength();
  next_src_pc_to_emit_ = start_pc + sequence_length;

  // Update usage counts for all jumps originating in the sequence we are about
  // to replace. Counts for the target locations are decremented here. Emitting
  // kJumpTarget below will again increment for newly emitted destinations.
  auto edge_it = src_writer_->jump_edges().lower_bound(start_pc);
  while (edge_it != src_writer_->jump_edges().end() &&
         edge_it->first < start_pc + sequence_length) {
    int target = edge_it->second;
    auto count_it = jump_usage_counts_.find(target);
    DCHECK_NE(count_it, jump_usage_counts_.end());
    count_it->second--;
    edge_it++;
  }

  int optimized_start_pc = pc();
  // List of offsets in the optimized sequence that need to be patched to the
  // offset value right after the optimized sequence.
  ZoneLinkedList<uint32_t> after_sequence_offsets(zone());

  const RegExpBytecode bc = last_node.OptimizedBytecode();
  dst_writer_->EmitBytecode(bc);

  for (size_t arg_idx = 0; arg_idx < last_node.ArgumentSize(); arg_idx++) {
    BytecodeArgumentMapping arg_map = last_node.ArgumentMapping(arg_idx);
    if (arg_map.type() == BytecodeArgumentMapping::Type::kDefault) {
      if (arg_map.new_operand_type() ==
          RegExpBytecodeOperandType::kJumpTarget) {
        int target = GetArgumentValue(bytecode, start_pc + arg_map.offset(),
                                      arg_map.length());
        jump_usage_counts_[target]++;
      }
      EmitArgument(start_pc, bytecode, arg_map);
    } else {
      DCHECK_EQ(arg_map.type(),
                BytecodeArgumentMapping::Type::kOffsetAfterSequence);
      after_sequence_offsets.push_back(optimized_start_pc +
                                       arg_map.new_offset());
      // Reserve space to overwrite later with the pc after this sequence.
      dst_writer_->Emit<uint32_t>(0, arg_map.new_offset());
    }
  }

  // Final alignment.
  dst_writer_->Finalize(bc);
  DCHECK_EQ(pc(), optimized_start_pc + RegExpBytecodes::Size(bc));

  int fixup_length = RegExpBytecodes::Size(bc) - sequence_length;

  // Check if there are any jumps inside the old sequence.
  // If so we have to keep the bytecodes that are jumped to around.
  auto jump_destination_candidate = jump_usage_counts_.upper_bound(start_pc);
  int jump_candidate_destination = jump_destination_candidate->first;
  int jump_candidate_count = jump_destination_candidate->second;
  // Jump destinations only jumped to from inside the sequence will be ignored.
  while (jump_destination_candidate != jump_usage_counts_.end() &&
         jump_candidate_count == 0) {
    ++jump_destination_candidate;
    jump_candidate_destination = jump_destination_candidate->first;
    jump_candidate_count = jump_destination_candidate->second;
  }

  int preserve_from = start_pc + sequence_length;
  if (jump_destination_candidate != jump_usage_counts_.end() &&
      jump_candidate_destination < start_pc + sequence_length) {
    preserve_from = jump_candidate_destination;
    // Check if any jump in the sequence we are preserving has a jump
    // destination inside the optimized sequence before the current position we
    // want to preserve. If so we have to preserve all bytecodes starting at
    // this jump destination.
    for (auto jump_iter = src_writer_->jump_edges().lower_bound(preserve_from);
         jump_iter != src_writer_->jump_edges().end() &&
         jump_iter->first /* jump source */ < start_pc + sequence_length;
         ++jump_iter) {
      int jump_destination = jump_iter->second;
      if (jump_destination > start_pc && jump_destination < preserve_from) {
        preserve_from = jump_destination;
      }
    }

    // We preserve everything to the end of the sequence. This is conservative
    // since it would be enough to preserve all bytecodes up to an unconditional
    // jump.
    int preserve_length = start_pc + sequence_length - preserve_from;
    fixup_length += preserve_length;
    // All jump targets after the start of the optimized sequence need to be
    // fixed relative to the length of the optimized sequence including
    // bytecodes we preserved.
    AddJumpDestinationFixup(fixup_length, start_pc + 1);
    // Jumps to the sequence we preserved need absolute fixup as they could
    // occur before or after the sequence.
    SetJumpDestinationFixup(pc() - preserve_from, preserve_from);
    dst_writer_->EmitRawBytecodeStream(src_writer_, preserve_from,
                                       preserve_length);
  } else {
    AddJumpDestinationFixup(fixup_length, start_pc + 1);
  }

  for (uint32_t offset : after_sequence_offsets) {
    DCHECK_EQ(dst_writer_->buffer()[offset], 0);
    dst_writer_->OverwriteValue<uint32_t>(pc(), offset);
  }
}

void RegExpBytecodePeephole::AddJumpDestinationFixup(int fixup, int pos) {
  auto previous_fixup = jump_destination_fixups_.lower_bound(pos);
  DCHECK(previous_fixup != jump_destination_fixups_.end());
  DCHECK(previous_fixup != jump_destination_fixups_.begin());

  int previous_fixup_value = (--previous_fixup)->second;
  jump_destination_fixups_[pos] = previous_fixup_value + fixup;
}

void RegExpBytecodePeephole::SetJumpDestinationFixup(int fixup, int pos) {
  auto previous_fixup = jump_destination_fixups_.lower_bound(pos);
  DCHECK(previous_fixup != jump_destination_fixups_.end());
  DCHECK(previous_fixup != jump_destination_fixups_.begin());

  int previous_fixup_value = (--previous_fixup)->second;
  jump_destination_fixups_.emplace(pos, fixup);
  jump_destination_fixups_.emplace(pos + 1, previous_fixup_value);
}

void RegExpBytecodePeephole::FixJumps() {
  for (auto jump_edge : dst_writer_->jump_edges()) {
    int jump_source = jump_edge.first;
    int jump_destination = jump_edge.second;
    int fixed_jump_destination =
        jump_destination +
        (--jump_destination_fixups_.upper_bound(jump_destination))->second;
    DCHECK_LT(fixed_jump_destination, pc());
    // TODO(pthier): This check could be better if we track the bytecodes
    // actually used and check if we jump to one of them.
    DCHECK(RegExpBytecodes::IsValidJumpTarget(
        dst_writer_->buffer()[fixed_jump_destination]));

    if (jump_destination != fixed_jump_destination) {
      dst_writer_->PatchJump(fixed_jump_destination, jump_source);
    }
  }
}

void RegExpBytecodePeephole::EmitArgument(int start_pc, const uint8_t* bytecode,
                                          BytecodeArgumentMapping arg) {
  const RegExpBytecodeOperandType type = arg.new_operand_type();

  switch (type) {
#define CASE(Name, ...)                                                      \
  case RegExpBytecodeOperandType::k##Name: {                                 \
    DCHECK_LE(arg.length(), kInt32Size);                                     \
    using CType =                                                            \
        RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name>::kCType; \
    CType value = static_cast<CType>(                                        \
        GetArgumentValue(bytecode, start_pc + arg.offset(), arg.length()));  \
    dst_writer_->EmitOperand<RegExpBytecodeOperandType::k##Name>(            \
        value, arg.new_offset());                                            \
  } break;
    BASIC_BYTECODE_OPERAND_TYPE_LIST(CASE)
    BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(CASE)
#undef CASE
    case RegExpBytecodeOperandType::kBitTable: {
      DCHECK_EQ(arg.length(), 16);
      dst_writer_->EmitOperand<RegExpBytecodeOperandType::kBitTable>(
          bytecode + start_pc + arg.offset(), arg.new_offset());
    } break;
    default:
      UNREACHABLE();
  }
}

int RegExpBytecodePeephole::pc() const { return dst_writer_->pc(); }

Zone* RegExpBytecodePeephole::zone() const { return zone_; }

}  // namespace

// static
DirectHandle<TrustedByteArray>
RegExpBytecodePeepholeOptimization::OptimizeBytecode(
    Isolate* isolate, Zone* zone, DirectHandle<String> source,
    RegExpBytecodeWriter* src_writer) {
  RegExpBytecodeWriter second_writer(zone);
  RegExpBytecodeWriter* dst_writer = &second_writer;

  // Preserve the original bytecode for tracing if needed.
  ZoneVector<uint8_t> original_bytecode(zone);
  if (v8_flags.trace_regexp_peephole_optimization) {
    original_bytecode = src_writer->buffer();
  }

  // Run the peephole optimizer until we've reached a fixed point. All relevant
  // data structures ping-pong between src and dst_writer.
  bool any_pass_optimized = false;
  for (;;) {
    dst_writer->Reset();
    // TODO(jgruber): This currently recreates standard definitions for each
    // pass. These should be global instead (or at the very least, once per
    // compilation).
    bool this_pass_optimized =
        RegExpBytecodePeephole::OptimizeBytecode(zone, src_writer, dst_writer);
    if (!this_pass_optimized) break;
    any_pass_optimized = true;
    std::swap(dst_writer, src_writer);
  }

  // The result is in the src_writer (not in dst, since the last pass did not
  // change anything).
  const uint8_t* optimized_bytecode = src_writer->buffer().data();
  int optimized_length = src_writer->length();

  DirectHandle<TrustedByteArray> array =
      isolate->factory()->NewTrustedByteArray(optimized_length);
  MemCopy(array->begin(), optimized_bytecode, optimized_length);

  if (any_pass_optimized && v8_flags.trace_regexp_peephole_optimization) {
    PrintF("Original Bytecode:\n");
    RegExpBytecodeDisassemble(original_bytecode.data(),
                              static_cast<int>(original_bytecode.size()),
                              source->ToCString().get());
    PrintF("Optimized Bytecode:\n");
    RegExpBytecodeDisassemble(array->begin(), optimized_length,
                              source->ToCString().get());
  }

  return array;
}

}  // namespace internal
}  // namespace v8
