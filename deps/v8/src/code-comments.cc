// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <iomanip>

#include "src/assembler-inl.h"
#include "src/code-comments.h"

namespace v8 {
namespace internal {

namespace {
static constexpr uint8_t kOffsetToFirstCommentEntry = kUInt32Size;
static constexpr uint8_t kOffsetToPCOffset = 0;
static constexpr uint8_t kOffsetToCommentSize = kOffsetToPCOffset + kUInt32Size;
static constexpr uint8_t kOffsetToCommentString =
    kOffsetToCommentSize + kUInt32Size;
}  // namespace

uint32_t CodeCommentEntry::comment_length() const {
  return static_cast<uint32_t>(comment.size() + 1);
}

uint32_t CodeCommentEntry::size() const {
  return kOffsetToCommentString + comment_length();
}

CodeCommentsIterator::CodeCommentsIterator(Address code_comments_start,
                                           uint32_t code_comments_size)
    : code_comments_start_(code_comments_start),
      code_comments_size_(code_comments_size),
      current_entry_(code_comments_start + kOffsetToFirstCommentEntry) {
  DCHECK_NE(kNullAddress, code_comments_start);
  DCHECK_IMPLIES(
      code_comments_size,
      code_comments_size == *reinterpret_cast<uint32_t*>(code_comments_start_));
}

uint32_t CodeCommentsIterator::size() const { return code_comments_size_; }

const char* CodeCommentsIterator::GetComment() const {
  const char* comment_string =
      reinterpret_cast<const char*>(current_entry_ + kOffsetToCommentString);
  CHECK_EQ(GetCommentSize(), strlen(comment_string) + 1);
  return comment_string;
}

uint32_t CodeCommentsIterator::GetCommentSize() const {
  return ReadUnalignedValue<uint32_t>(current_entry_ + kOffsetToCommentSize);
}

uint32_t CodeCommentsIterator::GetPCOffset() const {
  return ReadUnalignedValue<uint32_t>(current_entry_ + kOffsetToPCOffset);
}

void CodeCommentsIterator::Next() {
  current_entry_ += kOffsetToCommentString + GetCommentSize();
}

bool CodeCommentsIterator::HasCurrent() const {
  return current_entry_ < code_comments_start_ + size();
}

void CodeCommentsWriter::Emit(Assembler* assm) {
  assm->dd(section_size());
  for (auto i = comments_.begin(); i != comments_.end(); ++i) {
    assm->dd(i->pc_offset);
    assm->dd(i->comment_length());
    for (char c : i->comment) {
      EnsureSpace ensure_space(assm);
      assm->db(c);
    }
    assm->db('\0');
  }
}

void CodeCommentsWriter::Add(uint32_t pc_offset, std::string comment) {
  CodeCommentEntry entry = {pc_offset, std::move(comment)};
  byte_count_ += entry.size();
  comments_.push_back(std::move(entry));
}

size_t CodeCommentsWriter::entry_count() const { return comments_.size(); }
uint32_t CodeCommentsWriter::section_size() const {
  return kOffsetToFirstCommentEntry + static_cast<uint32_t>(byte_count_);
}

void PrintCodeCommentsSection(std::ostream& out, Address code_comments_start,
                              uint32_t code_comments_size) {
  CodeCommentsIterator it(code_comments_start, code_comments_size);
  out << "CodeComments (size = " << it.size() << ")\n";
  if (it.HasCurrent()) {
    out << std::setw(6) << "pc" << std::setw(6) << "len"
        << " comment\n";
  }
  for (; it.HasCurrent(); it.Next()) {
    out << std::hex << std::setw(6) << it.GetPCOffset() << std::dec
        << std::setw(6) << it.GetCommentSize() << " (" << it.GetComment()
        << ")\n";
  }
}

}  // namespace internal
}  // namespace v8
