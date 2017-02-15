// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/code-stats.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// Record code statisitcs.
void CodeStatistics::RecordCodeAndMetadataStatistics(HeapObject* object,
                                                     Isolate* isolate) {
  if (!object->IsAbstractCode()) {
    return;
  }

  // Record code+metadata statisitcs.
  AbstractCode* abstract_code = AbstractCode::cast(object);
  int size = abstract_code->SizeIncludingMetadata();
  if (abstract_code->IsCode()) {
    size += isolate->code_and_metadata_size();
    isolate->set_code_and_metadata_size(size);
  } else {
    size += isolate->bytecode_and_metadata_size();
    isolate->set_bytecode_and_metadata_size(size);
  }

#ifdef DEBUG
  // Record code kind and code comment statistics.
  isolate->code_kind_statistics()[abstract_code->kind()] +=
      abstract_code->Size();
  CodeStatistics::CollectCodeCommentStatistics(object, isolate);
#endif
}

void CodeStatistics::ResetCodeAndMetadataStatistics(Isolate* isolate) {
  isolate->set_code_and_metadata_size(0);
  isolate->set_bytecode_and_metadata_size(0);
#ifdef DEBUG
  ResetCodeStatistics(isolate);
#endif
}

// Collects code size statistics:
// - code and metadata size
// - by code kind (only in debug mode)
// - by code comment (only in debug mode)
void CodeStatistics::CollectCodeStatistics(PagedSpace* space,
                                           Isolate* isolate) {
  HeapObjectIterator obj_it(space);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next()) {
    RecordCodeAndMetadataStatistics(obj, isolate);
  }
}

// Collects code size statistics in LargeObjectSpace:
// - code and metadata size
// - by code kind (only in debug mode)
// - by code comment (only in debug mode)
void CodeStatistics::CollectCodeStatistics(LargeObjectSpace* space,
                                           Isolate* isolate) {
  LargeObjectIterator obj_it(space);
  for (HeapObject* obj = obj_it.Next(); obj != NULL; obj = obj_it.Next()) {
    RecordCodeAndMetadataStatistics(obj, isolate);
  }
}

#ifdef DEBUG
void CodeStatistics::ReportCodeStatistics(Isolate* isolate) {
  // Report code kind statistics
  int* code_kind_statistics = isolate->code_kind_statistics();
  PrintF("\n   Code kind histograms: \n");
  for (int i = 0; i < AbstractCode::NUMBER_OF_KINDS; i++) {
    if (code_kind_statistics[i] > 0) {
      PrintF("     %-20s: %10d bytes\n",
             AbstractCode::Kind2String(static_cast<AbstractCode::Kind>(i)),
             code_kind_statistics[i]);
    }
  }
  PrintF("\n");

  // Report code and metadata statisitcs
  if (isolate->code_and_metadata_size() > 0) {
    PrintF("Code size including metadata    : %10d bytes\n",
           isolate->code_and_metadata_size());
  }
  if (isolate->bytecode_and_metadata_size() > 0) {
    PrintF("Bytecode size including metadata: %10d bytes\n",
           isolate->bytecode_and_metadata_size());
  }

  // Report code comment statistics
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  PrintF(
      "Code comment statistics (\"   [ comment-txt   :    size/   "
      "count  (average)\"):\n");
  for (int i = 0; i <= CommentStatistic::kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size / cs.count);
    }
  }
  PrintF("\n");
}

void CodeStatistics::ResetCodeStatistics(Isolate* isolate) {
  // Clear code kind statistics
  int* code_kind_statistics = isolate->code_kind_statistics();
  for (int i = 0; i < AbstractCode::NUMBER_OF_KINDS; i++) {
    code_kind_statistics[i] = 0;
  }

  // Clear code comment statistics
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    comments_statistics[i].Clear();
  }
  comments_statistics[CommentStatistic::kMaxComments].comment = "Unknown";
  comments_statistics[CommentStatistic::kMaxComments].size = 0;
  comments_statistics[CommentStatistic::kMaxComments].count = 0;
}

// Adds comment to 'comment_statistics' table. Performance OK as long as
// 'kMaxComments' is small
void CodeStatistics::EnterComment(Isolate* isolate, const char* comment,
                                  int delta) {
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  // Do not count empty comments
  if (delta <= 0) return;
  CommentStatistic* cs = &comments_statistics[CommentStatistic::kMaxComments];
  // Search for a free or matching entry in 'comments_statistics': 'cs'
  // points to result.
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    if (comments_statistics[i].comment == NULL) {
      cs = &comments_statistics[i];
      cs->comment = comment;
      break;
    } else if (strcmp(comments_statistics[i].comment, comment) == 0) {
      cs = &comments_statistics[i];
      break;
    }
  }
  // Update entry for 'comment'
  cs->size += delta;
  cs->count += 1;
}

// Call for each nested comment start (start marked with '[ xxx', end marked
// with ']'.  RelocIterator 'it' must point to a comment reloc info.
void CodeStatistics::CollectCommentStatistics(Isolate* isolate,
                                              RelocIterator* it) {
  DCHECK(!it->done());
  DCHECK(it->rinfo()->rmode() == RelocInfo::COMMENT);
  const char* tmp = reinterpret_cast<const char*>(it->rinfo()->data());
  if (tmp[0] != '[') {
    // Not a nested comment; skip
    return;
  }

  // Search for end of nested comment or a new nested comment
  const char* const comment_txt =
      reinterpret_cast<const char*>(it->rinfo()->data());
  const byte* prev_pc = it->rinfo()->pc();
  int flat_delta = 0;
  it->next();
  while (true) {
    // All nested comments must be terminated properly, and therefore exit
    // from loop.
    DCHECK(!it->done());
    if (it->rinfo()->rmode() == RelocInfo::COMMENT) {
      const char* const txt =
          reinterpret_cast<const char*>(it->rinfo()->data());
      flat_delta += static_cast<int>(it->rinfo()->pc() - prev_pc);
      if (txt[0] == ']') break;  // End of nested  comment
      // A new comment
      CollectCommentStatistics(isolate, it);
      // Skip code that was covered with previous comment
      prev_pc = it->rinfo()->pc();
    }
    it->next();
  }
  EnterComment(isolate, comment_txt, flat_delta);
}

// Collects code comment statistics
void CodeStatistics::CollectCodeCommentStatistics(HeapObject* obj,
                                                  Isolate* isolate) {
  // Bytecode objects do not contain RelocInfo. Only process code objects
  // for code comment statistics.
  if (!obj->IsCode()) {
    return;
  }

  Code* code = Code::cast(obj);
  RelocIterator it(code);
  int delta = 0;
  const byte* prev_pc = code->instruction_start();
  while (!it.done()) {
    if (it.rinfo()->rmode() == RelocInfo::COMMENT) {
      delta += static_cast<int>(it.rinfo()->pc() - prev_pc);
      CollectCommentStatistics(isolate, &it);
      prev_pc = it.rinfo()->pc();
    }
    it.next();
  }

  DCHECK(code->instruction_start() <= prev_pc &&
         prev_pc <= code->instruction_end());
  delta += static_cast<int>(code->instruction_end() - prev_pc);
  EnterComment(isolate, "NoComment", delta);
}
#endif

}  // namespace internal
}  // namespace v8
