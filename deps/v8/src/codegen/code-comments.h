// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CODE_COMMENTS_H_
#define V8_CODEGEN_CODE_COMMENTS_H_

#include <ostream>
#include <string>
#include <vector>

#include "include/v8-internal.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Assembler;

// Code comments section layout:
// byte count              content
// ------------------------------------------------------------------------
// 4                       size as uint32_t (only for sanity check)
// [Inline array of CodeCommentEntry in increasing pc_offset order]
// ┌ 4                     pc_offset of entry as uint32_t
// ├ 4                     length of the comment including terminating '\0'
// └ <variable length>     characters of the comment including terminating '\0'

struct CodeCommentEntry {
  uint32_t pc_offset;
  std::string comment;
  uint32_t comment_length() const;
  uint32_t size() const;
};

class CodeCommentsWriter {
 public:
  V8_EXPORT_PRIVATE void Add(uint32_t pc_offset, std::string comment);
  void Emit(Assembler* assm);
  size_t entry_count() const;
  uint32_t section_size() const;

 private:
  uint32_t byte_count_ = 0;
  std::vector<CodeCommentEntry> comments_;
};

class V8_EXPORT_PRIVATE CodeCommentsIterator {
 public:
  CodeCommentsIterator(Address code_comments_start,
                       uint32_t code_comments_size);
  uint32_t size() const;
  const char* GetComment() const;
  uint32_t GetCommentSize() const;
  uint32_t GetPCOffset() const;
  void Next();
  bool HasCurrent() const;

 private:
  Address code_comments_start_;
  uint32_t code_comments_size_;
  Address current_entry_;
};

void PrintCodeCommentsSection(std::ostream& out, Address code_comments_start,
                              uint32_t code_comments_size);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_CODE_COMMENTS_H_
