// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FEEDBACK_SOURCE_H_
#define V8_COMPILER_FEEDBACK_SOURCE_H_

#include "src/compiler/heap-refs.h"
#include "src/objects/feedback-vector.h"

namespace v8 {
namespace internal {
namespace compiler {

struct FeedbackSource {
  FeedbackSource() { DCHECK(!IsValid()); }
  V8_EXPORT_PRIVATE FeedbackSource(IndirectHandle<FeedbackVector> vector_,
                                   FeedbackSlot slot_);
  FeedbackSource(FeedbackVectorRef vector_, FeedbackSlot slot_);

  bool IsValid() const { return !vector.is_null() && !slot.IsInvalid(); }
  int index() const;

  IndirectHandle<FeedbackVector> vector;
  FeedbackSlot slot;

  struct Hash {
    size_t operator()(FeedbackSource const& source) const {
      return base::hash_combine(source.vector.address(), source.slot);
    }
  };

  struct Equal {
    bool operator()(FeedbackSource const& lhs,
                    FeedbackSource const& rhs) const {
      return lhs.vector.equals(rhs.vector) && lhs.slot == rhs.slot;
    }
  };
};

V8_INLINE bool operator==(FeedbackSource const& lhs,
                          FeedbackSource const& rhs) {
  return FeedbackSource::Equal()(lhs, rhs);
}
bool operator!=(FeedbackSource const&, FeedbackSource const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           FeedbackSource const&);
inline size_t hash_value(const FeedbackSource& value) {
  return FeedbackSource::Hash()(value);
}

struct EmbeddedFeedbackSource {
  EmbeddedFeedbackSource() { DCHECK(!IsValid()); }
  V8_EXPORT_PRIVATE EmbeddedFeedbackSource(
      IndirectHandle<BytecodeArray> bytecode_array, int offset);

  bool IsValid() const { return !bytecode_array_.is_null(); }

  IndirectHandle<BytecodeArray> bytecode_array_;
  int offset_;

  struct Hash {
    size_t operator()(EmbeddedFeedbackSource const& source) const {
      return base::hash_combine(source.bytecode_array_.address(),
                                source.offset_);
    }
  };

  struct Equal {
    bool operator()(EmbeddedFeedbackSource const& lhs,
                    EmbeddedFeedbackSource const& rhs) const {
      return lhs.bytecode_array_.equals(rhs.bytecode_array_) &&
             lhs.offset_ == rhs.offset_;
    }
  };
};

V8_INLINE bool operator==(EmbeddedFeedbackSource const& lhs,
                          EmbeddedFeedbackSource const& rhs) {
  return EmbeddedFeedbackSource::Equal()(lhs, rhs);
}
V8_INLINE bool operator!=(EmbeddedFeedbackSource const&,
                          EmbeddedFeedbackSource const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           EmbeddedFeedbackSource const&);
inline size_t hash_value(const EmbeddedFeedbackSource& value) {
  return EmbeddedFeedbackSource::Hash()(value);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FEEDBACK_SOURCE_H_
