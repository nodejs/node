// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/liveedit.h"

#include "src/api/api-inl.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/source-position-table.h"
#include "src/common/globals.h"
#include "src/debug/debug-interface.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/v8threads.h"
#include "src/init/v8.h"
#include "src/logging/log.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"

namespace v8 {
namespace internal {
namespace {
// A general-purpose comparator between 2 arrays.
class Comparator {
 public:
  // Holds 2 arrays of some elements allowing to compare any pair of
  // element from the first array and element from the second array.
  class Input {
   public:
    virtual int GetLength1() = 0;
    virtual int GetLength2() = 0;
    virtual bool Equals(int index1, int index2) = 0;

   protected:
    virtual ~Input() = default;
  };

  // Receives compare result as a series of chunks.
  class Output {
   public:
    // Puts another chunk in result list. Note that technically speaking
    // only 3 arguments actually needed with 4th being derivable.
    virtual void AddChunk(int pos1, int pos2, int len1, int len2) = 0;

   protected:
    virtual ~Output() = default;
  };

  // Finds the difference between 2 arrays of elements.
  static void CalculateDifference(Input* input, Output* result_writer);
};

// A simple implementation of dynamic programming algorithm. It solves
// the problem of finding the difference of 2 arrays. It uses a table of results
// of subproblems. Each cell contains a number together with 2-bit flag
// that helps building the chunk list.
class Differencer {
 public:
  explicit Differencer(Comparator::Input* input)
      : input_(input), len1_(input->GetLength1()), len2_(input->GetLength2()) {
    buffer_ = NewArray<int>(len1_ * len2_);
  }
  ~Differencer() {
    DeleteArray(buffer_);
  }

  void Initialize() {
    int array_size = len1_ * len2_;
    for (int i = 0; i < array_size; i++) {
      buffer_[i] = kEmptyCellValue;
    }
  }

  // Makes sure that result for the full problem is calculated and stored
  // in the table together with flags showing a path through subproblems.
  void FillTable() {
    CompareUpToTail(0, 0);
  }

  void SaveResult(Comparator::Output* chunk_writer) {
    ResultWriter writer(chunk_writer);

    int pos1 = 0;
    int pos2 = 0;
    while (true) {
      if (pos1 < len1_) {
        if (pos2 < len2_) {
          Direction dir = get_direction(pos1, pos2);
          switch (dir) {
            case EQ:
              writer.eq();
              pos1++;
              pos2++;
              break;
            case SKIP1:
              writer.skip1(1);
              pos1++;
              break;
            case SKIP2:
            case SKIP_ANY:
              writer.skip2(1);
              pos2++;
              break;
            default:
              UNREACHABLE();
          }
        } else {
          writer.skip1(len1_ - pos1);
          break;
        }
      } else {
        if (len2_ != pos2) {
          writer.skip2(len2_ - pos2);
        }
        break;
      }
    }
    writer.close();
  }

 private:
  Comparator::Input* input_;
  int* buffer_;
  int len1_;
  int len2_;

  enum Direction {
    EQ = 0,
    SKIP1,
    SKIP2,
    SKIP_ANY,

    MAX_DIRECTION_FLAG_VALUE = SKIP_ANY
  };

  // Computes result for a subtask and optionally caches it in the buffer table.
  // All results values are shifted to make space for flags in the lower bits.
  int CompareUpToTail(int pos1, int pos2) {
    if (pos1 < len1_) {
      if (pos2 < len2_) {
        int cached_res = get_value4(pos1, pos2);
        if (cached_res == kEmptyCellValue) {
          Direction dir;
          int res;
          if (input_->Equals(pos1, pos2)) {
            res = CompareUpToTail(pos1 + 1, pos2 + 1);
            dir = EQ;
          } else {
            int res1 = CompareUpToTail(pos1 + 1, pos2) +
                (1 << kDirectionSizeBits);
            int res2 = CompareUpToTail(pos1, pos2 + 1) +
                (1 << kDirectionSizeBits);
            if (res1 == res2) {
              res = res1;
              dir = SKIP_ANY;
            } else if (res1 < res2) {
              res = res1;
              dir = SKIP1;
            } else {
              res = res2;
              dir = SKIP2;
            }
          }
          set_value4_and_dir(pos1, pos2, res, dir);
          cached_res = res;
        }
        return cached_res;
      } else {
        return (len1_ - pos1) << kDirectionSizeBits;
      }
    } else {
      return (len2_ - pos2) << kDirectionSizeBits;
    }
  }

  inline int& get_cell(int i1, int i2) {
    return buffer_[i1 + i2 * len1_];
  }

  // Each cell keeps a value plus direction. Value is multiplied by 4.
  void set_value4_and_dir(int i1, int i2, int value4, Direction dir) {
    DCHECK_EQ(0, value4 & kDirectionMask);
    get_cell(i1, i2) = value4 | dir;
  }

  int get_value4(int i1, int i2) {
    return get_cell(i1, i2) & (kMaxUInt32 ^ kDirectionMask);
  }
  Direction get_direction(int i1, int i2) {
    return static_cast<Direction>(get_cell(i1, i2) & kDirectionMask);
  }

  static const int kDirectionSizeBits = 2;
  static const int kDirectionMask = (1 << kDirectionSizeBits) - 1;
  static const int kEmptyCellValue = ~0u << kDirectionSizeBits;

  // This method only holds static assert statement (unfortunately you cannot
  // place one in class scope).
  void StaticAssertHolder() {
    STATIC_ASSERT(MAX_DIRECTION_FLAG_VALUE < (1 << kDirectionSizeBits));
  }

  class ResultWriter {
   public:
    explicit ResultWriter(Comparator::Output* chunk_writer)
        : chunk_writer_(chunk_writer), pos1_(0), pos2_(0),
          pos1_begin_(-1), pos2_begin_(-1), has_open_chunk_(false) {
    }
    void eq() {
      FlushChunk();
      pos1_++;
      pos2_++;
    }
    void skip1(int len1) {
      StartChunk();
      pos1_ += len1;
    }
    void skip2(int len2) {
      StartChunk();
      pos2_ += len2;
    }
    void close() {
      FlushChunk();
    }

   private:
    Comparator::Output* chunk_writer_;
    int pos1_;
    int pos2_;
    int pos1_begin_;
    int pos2_begin_;
    bool has_open_chunk_;

    void StartChunk() {
      if (!has_open_chunk_) {
        pos1_begin_ = pos1_;
        pos2_begin_ = pos2_;
        has_open_chunk_ = true;
      }
    }

    void FlushChunk() {
      if (has_open_chunk_) {
        chunk_writer_->AddChunk(pos1_begin_, pos2_begin_,
                                pos1_ - pos1_begin_, pos2_ - pos2_begin_);
        has_open_chunk_ = false;
      }
    }
  };
};

void Comparator::CalculateDifference(Comparator::Input* input,
                                     Comparator::Output* result_writer) {
  Differencer differencer(input);
  differencer.Initialize();
  differencer.FillTable();
  differencer.SaveResult(result_writer);
}

bool CompareSubstrings(Handle<String> s1, int pos1, Handle<String> s2, int pos2,
                       int len) {
  for (int i = 0; i < len; i++) {
    if (s1->Get(i + pos1) != s2->Get(i + pos2)) return false;
  }
  return true;
}

// Additional to Input interface. Lets switch Input range to subrange.
// More elegant way would be to wrap one Input as another Input object
// and translate positions there, but that would cost us additional virtual
// call per comparison.
class SubrangableInput : public Comparator::Input {
 public:
  virtual void SetSubrange1(int offset, int len) = 0;
  virtual void SetSubrange2(int offset, int len) = 0;
};


class SubrangableOutput : public Comparator::Output {
 public:
  virtual void SetSubrange1(int offset, int len) = 0;
  virtual void SetSubrange2(int offset, int len) = 0;
};

// Finds common prefix and suffix in input. This parts shouldn't take space in
// linear programming table. Enable subranging in input and output.
void NarrowDownInput(SubrangableInput* input, SubrangableOutput* output) {
  const int len1 = input->GetLength1();
  const int len2 = input->GetLength2();

  int common_prefix_len;
  int common_suffix_len;

  {
    common_prefix_len = 0;
    int prefix_limit = std::min(len1, len2);
    while (common_prefix_len < prefix_limit &&
        input->Equals(common_prefix_len, common_prefix_len)) {
      common_prefix_len++;
    }

    common_suffix_len = 0;
    int suffix_limit =
        std::min(len1 - common_prefix_len, len2 - common_prefix_len);

    while (common_suffix_len < suffix_limit &&
        input->Equals(len1 - common_suffix_len - 1,
        len2 - common_suffix_len - 1)) {
      common_suffix_len++;
    }
  }

  if (common_prefix_len > 0 || common_suffix_len > 0) {
    int new_len1 = len1 - common_suffix_len - common_prefix_len;
    int new_len2 = len2 - common_suffix_len - common_prefix_len;

    input->SetSubrange1(common_prefix_len, new_len1);
    input->SetSubrange2(common_prefix_len, new_len2);

    output->SetSubrange1(common_prefix_len, new_len1);
    output->SetSubrange2(common_prefix_len, new_len2);
  }
}

// Represents 2 strings as 2 arrays of tokens.
// TODO(LiveEdit): Currently it's actually an array of charactres.
//     Make array of tokens instead.
class TokensCompareInput : public Comparator::Input {
 public:
  TokensCompareInput(Handle<String> s1, int offset1, int len1,
                       Handle<String> s2, int offset2, int len2)
      : s1_(s1), offset1_(offset1), len1_(len1),
        s2_(s2), offset2_(offset2), len2_(len2) {
  }
  int GetLength1() override { return len1_; }
  int GetLength2() override { return len2_; }
  bool Equals(int index1, int index2) override {
    return s1_->Get(offset1_ + index1) == s2_->Get(offset2_ + index2);
  }

 private:
  Handle<String> s1_;
  int offset1_;
  int len1_;
  Handle<String> s2_;
  int offset2_;
  int len2_;
};

// Stores compare result in std::vector. Converts substring positions
// to absolute positions.
class TokensCompareOutput : public Comparator::Output {
 public:
  TokensCompareOutput(int offset1, int offset2,
                      std::vector<SourceChangeRange>* output)
      : output_(output), offset1_(offset1), offset2_(offset2) {}

  void AddChunk(int pos1, int pos2, int len1, int len2) override {
    output_->emplace_back(
        SourceChangeRange{pos1 + offset1_, pos1 + len1 + offset1_,
                          pos2 + offset2_, pos2 + offset2_ + len2});
  }

 private:
  std::vector<SourceChangeRange>* output_;
  int offset1_;
  int offset2_;
};

// Wraps raw n-elements line_ends array as a list of n+1 lines. The last line
// never has terminating new line character.
class LineEndsWrapper {
 public:
  explicit LineEndsWrapper(Isolate* isolate, Handle<String> string)
      : ends_array_(String::CalculateLineEnds(isolate, string, false)),
        string_len_(string->length()) {}
  int length() {
    return ends_array_->length() + 1;
  }
  // Returns start for any line including start of the imaginary line after
  // the last line.
  int GetLineStart(int index) { return index == 0 ? 0 : GetLineEnd(index - 1); }
  int GetLineEnd(int index) {
    if (index == ends_array_->length()) {
      // End of the last line is always an end of the whole string.
      // If the string ends with a new line character, the last line is an
      // empty string after this character.
      return string_len_;
    } else {
      return GetPosAfterNewLine(index);
    }
  }

 private:
  Handle<FixedArray> ends_array_;
  int string_len_;

  int GetPosAfterNewLine(int index) {
    return Smi::ToInt(ends_array_->get(index)) + 1;
  }
};

// Represents 2 strings as 2 arrays of lines.
class LineArrayCompareInput : public SubrangableInput {
 public:
  LineArrayCompareInput(Handle<String> s1, Handle<String> s2,
                        LineEndsWrapper line_ends1, LineEndsWrapper line_ends2)
      : s1_(s1), s2_(s2), line_ends1_(line_ends1),
        line_ends2_(line_ends2),
        subrange_offset1_(0), subrange_offset2_(0),
        subrange_len1_(line_ends1_.length()),
        subrange_len2_(line_ends2_.length()) {
  }
  int GetLength1() override { return subrange_len1_; }
  int GetLength2() override { return subrange_len2_; }
  bool Equals(int index1, int index2) override {
    index1 += subrange_offset1_;
    index2 += subrange_offset2_;

    int line_start1 = line_ends1_.GetLineStart(index1);
    int line_start2 = line_ends2_.GetLineStart(index2);
    int line_end1 = line_ends1_.GetLineEnd(index1);
    int line_end2 = line_ends2_.GetLineEnd(index2);
    int len1 = line_end1 - line_start1;
    int len2 = line_end2 - line_start2;
    if (len1 != len2) {
      return false;
    }
    return CompareSubstrings(s1_, line_start1, s2_, line_start2,
                             len1);
  }
  void SetSubrange1(int offset, int len) override {
    subrange_offset1_ = offset;
    subrange_len1_ = len;
  }
  void SetSubrange2(int offset, int len) override {
    subrange_offset2_ = offset;
    subrange_len2_ = len;
  }

 private:
  Handle<String> s1_;
  Handle<String> s2_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
  int subrange_offset1_;
  int subrange_offset2_;
  int subrange_len1_;
  int subrange_len2_;
};

// Stores compare result in std::vector. For each chunk tries to conduct
// a fine-grained nested diff token-wise.
class TokenizingLineArrayCompareOutput : public SubrangableOutput {
 public:
  TokenizingLineArrayCompareOutput(Isolate* isolate, LineEndsWrapper line_ends1,
                                   LineEndsWrapper line_ends2,
                                   Handle<String> s1, Handle<String> s2,
                                   std::vector<SourceChangeRange>* output)
      : isolate_(isolate),
        line_ends1_(line_ends1),
        line_ends2_(line_ends2),
        s1_(s1),
        s2_(s2),
        subrange_offset1_(0),
        subrange_offset2_(0),
        output_(output) {}

  void AddChunk(int line_pos1, int line_pos2, int line_len1,
                int line_len2) override {
    line_pos1 += subrange_offset1_;
    line_pos2 += subrange_offset2_;

    int char_pos1 = line_ends1_.GetLineStart(line_pos1);
    int char_pos2 = line_ends2_.GetLineStart(line_pos2);
    int char_len1 = line_ends1_.GetLineStart(line_pos1 + line_len1) - char_pos1;
    int char_len2 = line_ends2_.GetLineStart(line_pos2 + line_len2) - char_pos2;

    if (char_len1 < CHUNK_LEN_LIMIT && char_len2 < CHUNK_LEN_LIMIT) {
      // Chunk is small enough to conduct a nested token-level diff.
      HandleScope subTaskScope(isolate_);

      TokensCompareInput tokens_input(s1_, char_pos1, char_len1,
                                      s2_, char_pos2, char_len2);
      TokensCompareOutput tokens_output(char_pos1, char_pos2, output_);

      Comparator::CalculateDifference(&tokens_input, &tokens_output);
    } else {
      output_->emplace_back(SourceChangeRange{
          char_pos1, char_pos1 + char_len1, char_pos2, char_pos2 + char_len2});
    }
  }
  void SetSubrange1(int offset, int len) override {
    subrange_offset1_ = offset;
  }
  void SetSubrange2(int offset, int len) override {
    subrange_offset2_ = offset;
  }

 private:
  static const int CHUNK_LEN_LIMIT = 800;

  Isolate* isolate_;
  LineEndsWrapper line_ends1_;
  LineEndsWrapper line_ends2_;
  Handle<String> s1_;
  Handle<String> s2_;
  int subrange_offset1_;
  int subrange_offset2_;
  std::vector<SourceChangeRange>* output_;
};

struct SourcePositionEvent {
  enum Type { LITERAL_STARTS, LITERAL_ENDS, DIFF_STARTS, DIFF_ENDS };

  int position;
  Type type;

  union {
    FunctionLiteral* literal;
    int pos_diff;
  };

  SourcePositionEvent(FunctionLiteral* literal, bool is_start)
      : position(is_start ? literal->start_position()
                          : literal->end_position()),
        type(is_start ? LITERAL_STARTS : LITERAL_ENDS),
        literal(literal) {}
  SourcePositionEvent(const SourceChangeRange& change, bool is_start)
      : position(is_start ? change.start_position : change.end_position),
        type(is_start ? DIFF_STARTS : DIFF_ENDS),
        pos_diff((change.new_end_position - change.new_start_position) -
                 (change.end_position - change.start_position)) {}

  static bool LessThan(const SourcePositionEvent& a,
                       const SourcePositionEvent& b) {
    if (a.position != b.position) return a.position < b.position;
    if (a.type != b.type) return a.type < b.type;
    if (a.type == LITERAL_STARTS && b.type == LITERAL_STARTS) {
      // If the literals start in the same position, we want the one with the
      // furthest (i.e. largest) end position to be first.
      if (a.literal->end_position() != b.literal->end_position()) {
        return a.literal->end_position() > b.literal->end_position();
      }
      // If they also end in the same position, we want the first in order of
      // literal ids to be first.
      return a.literal->function_literal_id() <
             b.literal->function_literal_id();
    } else if (a.type == LITERAL_ENDS && b.type == LITERAL_ENDS) {
      // If the literals end in the same position, we want the one with the
      // nearest (i.e. largest) start position to be first.
      if (a.literal->start_position() != b.literal->start_position()) {
        return a.literal->start_position() > b.literal->start_position();
      }
      // If they also end in the same position, we want the last in order of
      // literal ids to be first.
      return a.literal->function_literal_id() >
             b.literal->function_literal_id();
    } else {
      return a.pos_diff < b.pos_diff;
    }
  }
};

struct FunctionLiteralChange {
  // If any of start/end position is kNoSourcePosition, this literal is
  // considered damaged and will not be mapped and edited at all.
  int new_start_position;
  int new_end_position;
  bool has_changes;
  FunctionLiteral* outer_literal;

  explicit FunctionLiteralChange(int new_start_position, FunctionLiteral* outer)
      : new_start_position(new_start_position),
        new_end_position(kNoSourcePosition),
        has_changes(false),
        outer_literal(outer) {}
};

using FunctionLiteralChanges =
    std::unordered_map<FunctionLiteral*, FunctionLiteralChange>;
void CalculateFunctionLiteralChanges(
    const std::vector<FunctionLiteral*>& literals,
    const std::vector<SourceChangeRange>& diffs,
    FunctionLiteralChanges* result) {
  std::vector<SourcePositionEvent> events;
  events.reserve(literals.size() * 2 + diffs.size() * 2);
  for (FunctionLiteral* literal : literals) {
    events.emplace_back(literal, true);
    events.emplace_back(literal, false);
  }
  for (const SourceChangeRange& diff : diffs) {
    events.emplace_back(diff, true);
    events.emplace_back(diff, false);
  }
  std::sort(events.begin(), events.end(), SourcePositionEvent::LessThan);
  bool inside_diff = false;
  int delta = 0;
  std::stack<std::pair<FunctionLiteral*, FunctionLiteralChange>> literal_stack;
  for (const SourcePositionEvent& event : events) {
    switch (event.type) {
      case SourcePositionEvent::DIFF_ENDS:
        DCHECK(inside_diff);
        inside_diff = false;
        delta += event.pos_diff;
        break;
      case SourcePositionEvent::LITERAL_ENDS: {
        DCHECK_EQ(literal_stack.top().first, event.literal);
        FunctionLiteralChange& change = literal_stack.top().second;
        change.new_end_position = inside_diff
                                      ? kNoSourcePosition
                                      : event.literal->end_position() + delta;
        result->insert(literal_stack.top());
        literal_stack.pop();
        break;
      }
      case SourcePositionEvent::LITERAL_STARTS:
        literal_stack.push(std::make_pair(
            event.literal,
            FunctionLiteralChange(
                inside_diff ? kNoSourcePosition
                            : event.literal->start_position() + delta,
                literal_stack.empty() ? nullptr : literal_stack.top().first)));
        break;
      case SourcePositionEvent::DIFF_STARTS:
        DCHECK(!inside_diff);
        inside_diff = true;
        if (!literal_stack.empty()) {
          // Note that outer literal has not necessarily changed, unless the
          // diff goes past the end of this literal. In this case, we'll mark
          // this function as damaged and parent as changed later in
          // MapLiterals.
          literal_stack.top().second.has_changes = true;
        }
        break;
    }
  }
}

// Function which has not changed itself, but if any variable in its
// outer context has been added/removed, we must consider this function
// as damaged and not update references to it.
// This is because old compiled function has hardcoded references to
// it's outer context.
bool HasChangedScope(FunctionLiteral* a, FunctionLiteral* b) {
  Scope* scope_a = a->scope()->outer_scope();
  Scope* scope_b = b->scope()->outer_scope();
  while (scope_a && scope_b) {
    std::unordered_map<int, Handle<String>> vars;
    for (Variable* var : *scope_a->locals()) {
      if (!var->IsContextSlot()) continue;
      vars[var->index()] = var->name();
    }
    for (Variable* var : *scope_b->locals()) {
      if (!var->IsContextSlot()) continue;
      auto it = vars.find(var->index());
      if (it == vars.end()) return true;
      if (*it->second != *var->name()) return true;
    }
    scope_a = scope_a->outer_scope();
    scope_b = scope_b->outer_scope();
  }
  return scope_a != scope_b;
}

enum ChangeState { UNCHANGED, CHANGED, DAMAGED };

using LiteralMap = std::unordered_map<FunctionLiteral*, FunctionLiteral*>;
void MapLiterals(const FunctionLiteralChanges& changes,
                 const std::vector<FunctionLiteral*>& new_literals,
                 LiteralMap* unchanged, LiteralMap* changed) {
  // Track the top-level script function separately as it can overlap fully with
  // another function, e.g. the script "()=>42".
  const std::pair<int, int> kTopLevelMarker = std::make_pair(-1, -1);
  std::map<std::pair<int, int>, FunctionLiteral*> position_to_new_literal;
  for (FunctionLiteral* literal : new_literals) {
    DCHECK(literal->start_position() != kNoSourcePosition);
    DCHECK(literal->end_position() != kNoSourcePosition);
    std::pair<int, int> key =
        literal->function_literal_id() == kFunctionLiteralIdTopLevel
            ? kTopLevelMarker
            : std::make_pair(literal->start_position(),
                             literal->end_position());
    // Make sure there are no duplicate keys.
    DCHECK_EQ(position_to_new_literal.find(key), position_to_new_literal.end());
    position_to_new_literal[key] = literal;
  }
  LiteralMap mappings;
  std::unordered_map<FunctionLiteral*, ChangeState> change_state;
  for (const auto& change_pair : changes) {
    FunctionLiteral* literal = change_pair.first;
    const FunctionLiteralChange& change = change_pair.second;
    std::pair<int, int> key =
        literal->function_literal_id() == kFunctionLiteralIdTopLevel
            ? kTopLevelMarker
            : std::make_pair(change.new_start_position,
                             change.new_end_position);
    auto it = position_to_new_literal.find(key);
    if (it == position_to_new_literal.end() ||
        HasChangedScope(literal, it->second)) {
      change_state[literal] = ChangeState::DAMAGED;
      if (!change.outer_literal) continue;
      if (change_state[change.outer_literal] != ChangeState::DAMAGED) {
        change_state[change.outer_literal] = ChangeState::CHANGED;
      }
    } else {
      mappings[literal] = it->second;
      if (change_state.find(literal) == change_state.end()) {
        change_state[literal] =
            change.has_changes ? ChangeState::CHANGED : ChangeState::UNCHANGED;
      }
    }
  }
  for (const auto& mapping : mappings) {
    if (change_state[mapping.first] == ChangeState::UNCHANGED) {
      (*unchanged)[mapping.first] = mapping.second;
    } else if (change_state[mapping.first] == ChangeState::CHANGED) {
      (*changed)[mapping.first] = mapping.second;
    }
  }
}

class CollectFunctionLiterals final
    : public AstTraversalVisitor<CollectFunctionLiterals> {
 public:
  CollectFunctionLiterals(Isolate* isolate, AstNode* root)
      : AstTraversalVisitor<CollectFunctionLiterals>(isolate, root) {}
  void VisitFunctionLiteral(FunctionLiteral* lit) {
    AstTraversalVisitor::VisitFunctionLiteral(lit);
    literals_->push_back(lit);
  }
  void Run(std::vector<FunctionLiteral*>* literals) {
    literals_ = literals;
    AstTraversalVisitor::Run();
    literals_ = nullptr;
  }

 private:
  std::vector<FunctionLiteral*>* literals_;
};

bool ParseScript(Isolate* isolate, Handle<Script> script, ParseInfo* parse_info,
                 bool compile_as_well, std::vector<FunctionLiteral*>* literals,
                 debug::LiveEditResult* result) {
  parse_info->set_eager();
  v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
  Handle<SharedFunctionInfo> shared;
  bool success = false;
  if (compile_as_well) {
    success = Compiler::CompileForLiveEdit(parse_info, script, isolate)
                  .ToHandle(&shared);
  } else {
    success = parsing::ParseProgram(parse_info, script, isolate);
    if (success) {
      success = Compiler::Analyze(parse_info);
      parse_info->ast_value_factory()->Internalize(isolate);
    }
  }
  if (!success) {
    isolate->OptionalRescheduleException(false);
    DCHECK(try_catch.HasCaught());
    result->message = try_catch.Message()->Get();
    auto self = Utils::OpenHandle(*try_catch.Message());
    auto msg = i::Handle<i::JSMessageObject>::cast(self);
    result->line_number = msg->GetLineNumber();
    result->column_number = msg->GetColumnNumber();
    result->status = debug::LiveEditResult::COMPILE_ERROR;
    return false;
  }
  CollectFunctionLiterals(isolate, parse_info->literal()).Run(literals);
  return true;
}

struct FunctionData {
  FunctionData(FunctionLiteral* literal, bool should_restart)
      : literal(literal),
        stack_position(NOT_ON_STACK),
        should_restart(should_restart) {}

  FunctionLiteral* literal;
  MaybeHandle<SharedFunctionInfo> shared;
  std::vector<Handle<JSFunction>> js_functions;
  std::vector<Handle<JSGeneratorObject>> running_generators;
  // In case of multiple functions with different stack position, the latest
  // one (in the order below) is used, since it is the most restrictive.
  // This is important only for functions to be restarted.
  enum StackPosition {
    NOT_ON_STACK,
    ABOVE_BREAK_FRAME,
    PATCHABLE,
    BELOW_NON_DROPPABLE_FRAME,
    ARCHIVED_THREAD,
  };
  StackPosition stack_position;
  bool should_restart;
};

class FunctionDataMap : public ThreadVisitor {
 public:
  void AddInterestingLiteral(int script_id, FunctionLiteral* literal,
                             bool should_restart) {
    map_.emplace(GetFuncId(script_id, literal),
                 FunctionData{literal, should_restart});
  }

  bool Lookup(SharedFunctionInfo sfi, FunctionData** data) {
    int start_position = sfi.StartPosition();
    if (!sfi.script().IsScript() || start_position == -1) {
      return false;
    }
    Script script = Script::cast(sfi.script());
    return Lookup(GetFuncId(script.id(), sfi), data);
  }

  bool Lookup(Handle<Script> script, FunctionLiteral* literal,
              FunctionData** data) {
    return Lookup(GetFuncId(script->id(), literal), data);
  }

  void Fill(Isolate* isolate, Address* restart_frame_fp) {
    {
      HeapObjectIterator iterator(isolate->heap(),
                                  HeapObjectIterator::kFilterUnreachable);
      for (HeapObject obj = iterator.Next(); !obj.is_null();
           obj = iterator.Next()) {
        if (obj.IsSharedFunctionInfo()) {
          SharedFunctionInfo sfi = SharedFunctionInfo::cast(obj);
          FunctionData* data = nullptr;
          if (!Lookup(sfi, &data)) continue;
          data->shared = handle(sfi, isolate);
        } else if (obj.IsJSFunction()) {
          JSFunction js_function = JSFunction::cast(obj);
          SharedFunctionInfo sfi = js_function.shared();
          FunctionData* data = nullptr;
          if (!Lookup(sfi, &data)) continue;
          data->js_functions.emplace_back(js_function, isolate);
        } else if (obj.IsJSGeneratorObject()) {
          JSGeneratorObject gen = JSGeneratorObject::cast(obj);
          if (gen.is_closed()) continue;
          SharedFunctionInfo sfi = gen.function().shared();
          FunctionData* data = nullptr;
          if (!Lookup(sfi, &data)) continue;
          data->running_generators.emplace_back(gen, isolate);
        }
      }
    }
    FunctionData::StackPosition stack_position =
        isolate->debug()->break_frame_id() == StackFrameId::NO_ID
            ? FunctionData::PATCHABLE
            : FunctionData::ABOVE_BREAK_FRAME;
    for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
      StackFrame* frame = it.frame();
      if (stack_position == FunctionData::ABOVE_BREAK_FRAME) {
        if (frame->id() == isolate->debug()->break_frame_id()) {
          stack_position = FunctionData::PATCHABLE;
        }
      }
      if (stack_position == FunctionData::PATCHABLE &&
          (frame->is_exit() || frame->is_builtin_exit())) {
        stack_position = FunctionData::BELOW_NON_DROPPABLE_FRAME;
        continue;
      }
      if (!frame->is_java_script()) continue;
      std::vector<Handle<SharedFunctionInfo>> sfis;
      JavaScriptFrame::cast(frame)->GetFunctions(&sfis);
      for (auto& sfi : sfis) {
        if (stack_position == FunctionData::PATCHABLE &&
            IsResumableFunction(sfi->kind())) {
          stack_position = FunctionData::BELOW_NON_DROPPABLE_FRAME;
        }
        FunctionData* data = nullptr;
        if (!Lookup(*sfi, &data)) continue;
        if (!data->should_restart) continue;
        data->stack_position = stack_position;
        *restart_frame_fp = frame->fp();
      }
    }

    isolate->thread_manager()->IterateArchivedThreads(this);
  }

 private:
  // Unique id for a function: script_id + start_position, where start_position
  // is special cased to -1 for top-level so that it does not overlap with a
  // function whose start position is 0.
  using FuncId = std::pair<int, int>;

  FuncId GetFuncId(int script_id, FunctionLiteral* literal) {
    int start_position = literal->start_position();
    if (literal->function_literal_id() == 0) {
      // This is the top-level script function literal, so special case its
      // start position
      DCHECK_EQ(start_position, 0);
      start_position = -1;
    }
    return FuncId(script_id, start_position);
  }

  FuncId GetFuncId(int script_id, SharedFunctionInfo sfi) {
    DCHECK_EQ(script_id, Script::cast(sfi.script()).id());
    int start_position = sfi.StartPosition();
    DCHECK_NE(start_position, -1);
    if (sfi.is_toplevel()) {
      // This is the top-level function, so special case its start position
      DCHECK_EQ(start_position, 0);
      start_position = -1;
    }
    return FuncId(script_id, start_position);
  }

  bool Lookup(FuncId id, FunctionData** data) {
    auto it = map_.find(id);
    if (it == map_.end()) return false;
    *data = &it->second;
    return true;
  }

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
    for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
      std::vector<Handle<SharedFunctionInfo>> sfis;
      it.frame()->GetFunctions(&sfis);
      for (auto& sfi : sfis) {
        FunctionData* data = nullptr;
        if (!Lookup(*sfi, &data)) continue;
        data->stack_position = FunctionData::ARCHIVED_THREAD;
      }
    }
  }

  std::map<FuncId, FunctionData> map_;
};

bool CanPatchScript(
    const LiteralMap& changed, Handle<Script> script, Handle<Script> new_script,
    FunctionDataMap& function_data_map,  // NOLINT(runtime/references)
    debug::LiveEditResult* result) {
  debug::LiveEditResult::Status status = debug::LiveEditResult::OK;
  for (const auto& mapping : changed) {
    FunctionData* data = nullptr;
    function_data_map.Lookup(script, mapping.first, &data);
    FunctionData* new_data = nullptr;
    function_data_map.Lookup(new_script, mapping.second, &new_data);
    Handle<SharedFunctionInfo> sfi;
    if (!data->shared.ToHandle(&sfi)) {
      continue;
    } else if (!data->should_restart) {
      UNREACHABLE();
    } else if (data->stack_position == FunctionData::ABOVE_BREAK_FRAME) {
      status = debug::LiveEditResult::BLOCKED_BY_FUNCTION_ABOVE_BREAK_FRAME;
    } else if (data->stack_position ==
               FunctionData::BELOW_NON_DROPPABLE_FRAME) {
      status =
          debug::LiveEditResult::BLOCKED_BY_FUNCTION_BELOW_NON_DROPPABLE_FRAME;
    } else if (!data->running_generators.empty()) {
      status = debug::LiveEditResult::BLOCKED_BY_RUNNING_GENERATOR;
    } else if (data->stack_position == FunctionData::ARCHIVED_THREAD) {
      status = debug::LiveEditResult::BLOCKED_BY_ACTIVE_FUNCTION;
    }
    if (status != debug::LiveEditResult::OK) {
      result->status = status;
      return false;
    }
  }
  return true;
}

bool CanRestartFrame(
    Isolate* isolate, Address fp,
    FunctionDataMap& function_data_map,  // NOLINT(runtime/references)
    const LiteralMap& changed, debug::LiveEditResult* result) {
  DCHECK_GT(fp, 0);
  StackFrame* restart_frame = nullptr;
  StackFrameIterator it(isolate);
  for (; !it.done(); it.Advance()) {
    if (it.frame()->fp() == fp) {
      restart_frame = it.frame();
      break;
    }
  }
  DCHECK(restart_frame && restart_frame->is_java_script());
  if (!LiveEdit::kFrameDropperSupported) {
    result->status = debug::LiveEditResult::FRAME_RESTART_IS_NOT_SUPPORTED;
    return false;
  }
  std::vector<Handle<SharedFunctionInfo>> sfis;
  JavaScriptFrame::cast(restart_frame)->GetFunctions(&sfis);
  for (auto& sfi : sfis) {
    FunctionData* data = nullptr;
    if (!function_data_map.Lookup(*sfi, &data)) continue;
    auto new_literal_it = changed.find(data->literal);
    if (new_literal_it == changed.end()) continue;
    if (new_literal_it->second->scope()->new_target_var()) {
      result->status =
          debug::LiveEditResult::BLOCKED_BY_NEW_TARGET_IN_RESTART_FRAME;
      return false;
    }
  }
  return true;
}

void TranslateSourcePositionTable(Isolate* isolate, Handle<BytecodeArray> code,
                                  const std::vector<SourceChangeRange>& diffs) {
  SourcePositionTableBuilder builder;

  Handle<ByteArray> source_position_table(code->SourcePositionTable(), isolate);
  for (SourcePositionTableIterator iterator(*source_position_table);
       !iterator.done(); iterator.Advance()) {
    SourcePosition position = iterator.source_position();
    position.SetScriptOffset(
        LiveEdit::TranslatePosition(diffs, position.ScriptOffset()));
    builder.AddPosition(iterator.code_offset(), position,
                        iterator.is_statement());
  }

  Handle<ByteArray> new_source_position_table(
      builder.ToSourcePositionTable(isolate));
  code->set_source_position_table(*new_source_position_table);
  LOG_CODE_EVENT(isolate,
                 CodeLinePosInfoRecordEvent(code->GetFirstBytecodeAddress(),
                                            *new_source_position_table));
}

void UpdatePositions(Isolate* isolate, Handle<SharedFunctionInfo> sfi,
                     const std::vector<SourceChangeRange>& diffs) {
  int old_start_position = sfi->StartPosition();
  int new_start_position =
      LiveEdit::TranslatePosition(diffs, old_start_position);
  int new_end_position = LiveEdit::TranslatePosition(diffs, sfi->EndPosition());
  int new_function_token_position =
      LiveEdit::TranslatePosition(diffs, sfi->function_token_position());
  sfi->SetPosition(new_start_position, new_end_position);
  sfi->SetFunctionTokenPosition(new_function_token_position,
                                new_start_position);
  if (sfi->HasBytecodeArray()) {
    TranslateSourcePositionTable(
        isolate, handle(sfi->GetBytecodeArray(), isolate), diffs);
  }
}
}  // anonymous namespace

void LiveEdit::PatchScript(Isolate* isolate, Handle<Script> script,
                           Handle<String> new_source, bool preview,
                           debug::LiveEditResult* result) {
  std::vector<SourceChangeRange> diffs;
  LiveEdit::CompareStrings(isolate,
                           handle(String::cast(script->source()), isolate),
                           new_source, &diffs);
  if (diffs.empty()) {
    result->status = debug::LiveEditResult::OK;
    return;
  }

  ParseInfo parse_info(isolate, *script);
  std::vector<FunctionLiteral*> literals;
  if (!ParseScript(isolate, script, &parse_info, false, &literals, result))
    return;

  Handle<Script> new_script = isolate->factory()->CloneScript(script);
  new_script->set_source(*new_source);
  std::vector<FunctionLiteral*> new_literals;
  ParseInfo new_parse_info(isolate, *new_script);
  if (!ParseScript(isolate, new_script, &new_parse_info, true, &new_literals,
                   result)) {
    return;
  }

  FunctionLiteralChanges literal_changes;
  CalculateFunctionLiteralChanges(literals, diffs, &literal_changes);

  LiteralMap changed;
  LiteralMap unchanged;
  MapLiterals(literal_changes, new_literals, &unchanged, &changed);

  FunctionDataMap function_data_map;
  for (const auto& mapping : changed) {
    function_data_map.AddInterestingLiteral(script->id(), mapping.first, true);
    function_data_map.AddInterestingLiteral(new_script->id(), mapping.second,
                                            false);
  }
  for (const auto& mapping : unchanged) {
    function_data_map.AddInterestingLiteral(script->id(), mapping.first, false);
  }
  Address restart_frame_fp = 0;
  function_data_map.Fill(isolate, &restart_frame_fp);

  if (!CanPatchScript(changed, script, new_script, function_data_map, result)) {
    return;
  }
  if (restart_frame_fp &&
      !CanRestartFrame(isolate, restart_frame_fp, function_data_map, changed,
                       result)) {
    return;
  }

  if (preview) {
    result->status = debug::LiveEditResult::OK;
    return;
  }

  std::map<int, int> start_position_to_unchanged_id;
  for (const auto& mapping : unchanged) {
    FunctionData* data = nullptr;
    if (!function_data_map.Lookup(script, mapping.first, &data)) continue;
    Handle<SharedFunctionInfo> sfi;
    if (!data->shared.ToHandle(&sfi)) continue;
    DCHECK_EQ(sfi->script(), *script);

    isolate->compilation_cache()->Remove(sfi);
    isolate->debug()->DeoptimizeFunction(sfi);
    if (sfi->HasDebugInfo()) {
      Handle<DebugInfo> debug_info(sfi->GetDebugInfo(), isolate);
      isolate->debug()->RemoveBreakInfoAndMaybeFree(debug_info);
    }
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, sfi);
    UpdatePositions(isolate, sfi, diffs);

    sfi->set_script(*new_script);
    sfi->set_function_literal_id(mapping.second->function_literal_id());
    new_script->shared_function_infos().Set(
        mapping.second->function_literal_id(), HeapObjectReference::Weak(*sfi));
    DCHECK_EQ(sfi->function_literal_id(),
              mapping.second->function_literal_id());

    // Save the new start_position -> id mapping, so that we can recover it when
    // iterating over changed functions' constant pools.
    start_position_to_unchanged_id[mapping.second->start_position()] =
        mapping.second->function_literal_id();

    if (sfi->HasUncompiledDataWithPreparseData()) {
      sfi->ClearPreparseData();
    }

    for (auto& js_function : data->js_functions) {
      js_function->set_raw_feedback_cell(
          *isolate->factory()->many_closures_cell());
      if (!js_function->is_compiled()) continue;
      JSFunction::EnsureFeedbackVector(js_function);
    }

    if (!sfi->HasBytecodeArray()) continue;
    FixedArray constants = sfi->GetBytecodeArray().constant_pool();
    for (int i = 0; i < constants.length(); ++i) {
      if (!constants.get(i).IsSharedFunctionInfo()) continue;
      FunctionData* data = nullptr;
      if (!function_data_map.Lookup(SharedFunctionInfo::cast(constants.get(i)),
                                    &data)) {
        continue;
      }
      auto change_it = changed.find(data->literal);
      if (change_it == changed.end()) continue;
      if (!function_data_map.Lookup(new_script, change_it->second, &data)) {
        continue;
      }
      Handle<SharedFunctionInfo> new_sfi;
      if (!data->shared.ToHandle(&new_sfi)) continue;
      constants.set(i, *new_sfi);
    }
  }
  for (const auto& mapping : changed) {
    FunctionData* data = nullptr;
    if (!function_data_map.Lookup(new_script, mapping.second, &data)) continue;
    Handle<SharedFunctionInfo> new_sfi = data->shared.ToHandleChecked();
    DCHECK_EQ(new_sfi->script(), *new_script);

    if (!function_data_map.Lookup(script, mapping.first, &data)) continue;
    Handle<SharedFunctionInfo> sfi;
    if (!data->shared.ToHandle(&sfi)) continue;

    isolate->debug()->DeoptimizeFunction(sfi);
    isolate->compilation_cache()->Remove(sfi);
    for (auto& js_function : data->js_functions) {
      js_function->set_shared(*new_sfi);
      js_function->set_code(js_function->shared().GetCode());

      js_function->set_raw_feedback_cell(
          *isolate->factory()->many_closures_cell());
      if (!js_function->is_compiled()) continue;
      JSFunction::EnsureFeedbackVector(js_function);
    }
  }
  SharedFunctionInfo::ScriptIterator it(isolate, *new_script);
  for (SharedFunctionInfo sfi = it.Next(); !sfi.is_null(); sfi = it.Next()) {
    if (!sfi.HasBytecodeArray()) continue;
    FixedArray constants = sfi.GetBytecodeArray().constant_pool();
    for (int i = 0; i < constants.length(); ++i) {
      if (!constants.get(i).IsSharedFunctionInfo()) continue;
      SharedFunctionInfo inner_sfi = SharedFunctionInfo::cast(constants.get(i));
      // See if there is a mapping from this function's start position to a
      // unchanged function's id.
      auto unchanged_it =
          start_position_to_unchanged_id.find(inner_sfi.StartPosition());
      if (unchanged_it == start_position_to_unchanged_id.end()) continue;

      // Grab that function id from the new script's SFI list, which should have
      // already been updated in in the unchanged pass.
      SharedFunctionInfo old_unchanged_inner_sfi =
          SharedFunctionInfo::cast(new_script->shared_function_infos()
                                       .Get(unchanged_it->second)
                                       ->GetHeapObject());
      if (old_unchanged_inner_sfi == inner_sfi) continue;
      DCHECK_NE(old_unchanged_inner_sfi, inner_sfi);
      // Now some sanity checks. Make sure that the unchanged SFI has already
      // been processed and patched to be on the new script ...
      DCHECK_EQ(old_unchanged_inner_sfi.script(), *new_script);
      constants.set(i, old_unchanged_inner_sfi);
    }
  }
#ifdef DEBUG
  {
    // Check that all the functions in the new script are valid, that their
    // function literals match what is expected, and that start positions are
    // unique.
    DisallowHeapAllocation no_gc;

    SharedFunctionInfo::ScriptIterator it(isolate, *new_script);
    std::set<int> start_positions;
    for (SharedFunctionInfo sfi = it.Next(); !sfi.is_null(); sfi = it.Next()) {
      DCHECK_EQ(sfi.script(), *new_script);
      DCHECK_EQ(sfi.function_literal_id(), it.CurrentIndex());
      // Don't check the start position of the top-level function, as it can
      // overlap with a function in the script.
      if (sfi.is_toplevel()) {
        DCHECK_EQ(start_positions.find(sfi.StartPosition()),
                  start_positions.end());
        start_positions.insert(sfi.StartPosition());
      }

      if (!sfi.HasBytecodeArray()) continue;
      // Check that all the functions in this function's constant pool are also
      // on the new script, and that their id matches their index in the new
      // scripts function list.
      FixedArray constants = sfi.GetBytecodeArray().constant_pool();
      for (int i = 0; i < constants.length(); ++i) {
        if (!constants.get(i).IsSharedFunctionInfo()) continue;
        SharedFunctionInfo inner_sfi =
            SharedFunctionInfo::cast(constants.get(i));
        DCHECK_EQ(inner_sfi.script(), *new_script);
        DCHECK_EQ(inner_sfi, new_script->shared_function_infos()
                                 .Get(inner_sfi.function_literal_id())
                                 ->GetHeapObject());
      }
    }
  }
#endif

  if (restart_frame_fp) {
    for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
      if (it.frame()->fp() == restart_frame_fp) {
        isolate->debug()->ScheduleFrameRestart(it.frame());
        result->stack_changed = true;
        break;
      }
    }
  }

  int script_id = script->id();
  script->set_id(new_script->id());
  new_script->set_id(script_id);
  result->status = debug::LiveEditResult::OK;
  result->script = ToApiHandle<v8::debug::Script>(new_script);
}

void LiveEdit::InitializeThreadLocal(Debug* debug) {
  debug->thread_local_.restart_fp_ = 0;
}

bool LiveEdit::RestartFrame(JavaScriptFrame* frame) {
  if (!LiveEdit::kFrameDropperSupported) return false;
  Isolate* isolate = frame->isolate();
  StackFrameId break_frame_id = isolate->debug()->break_frame_id();
  bool break_frame_found = break_frame_id == StackFrameId::NO_ID;
  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    StackFrame* current = it.frame();
    break_frame_found = break_frame_found || break_frame_id == current->id();
    if (current->fp() == frame->fp()) {
      if (break_frame_found) {
        isolate->debug()->ScheduleFrameRestart(current);
        return true;
      } else {
        return false;
      }
    }
    if (!break_frame_found) continue;
    if (current->is_exit() || current->is_builtin_exit()) {
      return false;
    }
    if (!current->is_java_script()) continue;
    std::vector<Handle<SharedFunctionInfo>> shareds;
    JavaScriptFrame::cast(current)->GetFunctions(&shareds);
    for (auto& shared : shareds) {
      if (IsResumableFunction(shared->kind())) {
        return false;
      }
    }
  }
  return false;
}

void LiveEdit::CompareStrings(Isolate* isolate, Handle<String> s1,
                              Handle<String> s2,
                              std::vector<SourceChangeRange>* diffs) {
  s1 = String::Flatten(isolate, s1);
  s2 = String::Flatten(isolate, s2);

  LineEndsWrapper line_ends1(isolate, s1);
  LineEndsWrapper line_ends2(isolate, s2);

  LineArrayCompareInput input(s1, s2, line_ends1, line_ends2);
  TokenizingLineArrayCompareOutput output(isolate, line_ends1, line_ends2, s1,
                                          s2, diffs);

  NarrowDownInput(&input, &output);

  Comparator::CalculateDifference(&input, &output);
}

int LiveEdit::TranslatePosition(const std::vector<SourceChangeRange>& diffs,
                                int position) {
  auto it = std::lower_bound(diffs.begin(), diffs.end(), position,
                             [](const SourceChangeRange& change, int position) {
                               return change.end_position < position;
                             });
  if (it != diffs.end() && position == it->end_position) {
    return it->new_end_position;
  }
  if (it == diffs.begin()) return position;
  DCHECK(it == diffs.end() || position <= it->start_position);
  it = std::prev(it);
  return position + (it->new_end_position - it->end_position);
}
}  // namespace internal
}  // namespace v8
