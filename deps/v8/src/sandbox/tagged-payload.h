// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TAGGED_PAYLOAD_H_
#define V8_SANDBOX_TAGGED_PAYLOAD_H_

#include "include/v8-sandbox.h"
#include "src/common/globals.h"
#include "src/sandbox/check.h"
#include "src/sandbox/indirect-pointer-tag.h"

namespace v8 {
namespace internal {

// A generic payload struct to use for the entries of a pointer table.
// It encodes (1) the pointer (2) the type tag and (3) the marking bit. The
// exact layout of the pointer, tag, and marking bit within the word is
// determined by the `PayloadTaggingScheme` template parameter, making this
// struct highly flexible and configurable for different requirements.
template <typename PayloadTaggingScheme>
struct TaggedPayload {
  using TagType = typename PayloadTaggingScheme::TagType;
  using TagRangeType = TagRange<TagType>;

  static constexpr uint64_t kTagShift = PayloadTaggingScheme::kTagShift;
  static constexpr uint64_t kPayloadShift = PayloadTaggingScheme::kPayloadShift;
  static constexpr uint64_t kPayloadMask = PayloadTaggingScheme::kPayloadMask;
  static constexpr uint64_t kTagMask = PayloadTaggingScheme::kTagMask;
  static constexpr uint64_t kMarkBit = PayloadTaggingScheme::kMarkBit;
  static constexpr TagType kFreeEntryTag = PayloadTaggingScheme::kFreeEntryTag;
  static constexpr TagType kEvacuationEntryTag =
      PayloadTaggingScheme::kEvacuationEntryTag;

  TaggedPayload(Address pointer, TagType tag)
      : encoded_word_(Tag(pointer, tag)) {}

  static Address Tag(Address pointer, TagType tag) {
    return (pointer << kPayloadShift) |
           (static_cast<Address>(tag) << kTagShift);
  }

  static bool CheckTag(Address content, TagRangeType tag_range) {
    // TODO(saelo): we should probably also check that the bits that are neither
    // part of the tag nor the payload are zero.
    TagType tag = static_cast<TagType>((content & kTagMask) >> kTagShift);
    return tag_range.Contains(tag);
  }

  Address UntagAllowNullHandle(TagRangeType tag_range) const {
    Address content = encoded_word_;
    if (V8_LIKELY(CheckTag(content, tag_range))) {
      return (content & kPayloadMask) >> kPayloadShift;
    } else {
      return kNullAddress;
    }
  }

  Address UntagDisallowNullHandle(TagRangeType tag_range) const {
    Address content = encoded_word_;
    SBXCHECK(CheckTag(content, tag_range));
    return (content & kPayloadMask) >> kPayloadShift;
  }

  Address Untag(TagRangeType tag_range) const {
    if (ExternalPointerCanBeEmpty(tag_range)) {
      // TODO(saelo): use well-known null entries per type tag instead of a
      // generic null entry. Then this logic can be removed entirely.
      return UntagAllowNullHandle(tag_range);
    } else {
      return UntagDisallowNullHandle(tag_range);
    }
  }

  Address Untag(TagType tag) const { return Untag(TagRangeType(tag, tag)); }

  bool IsTaggedWithTagIn(TagRangeType tag_range) const {
    return CheckTag(encoded_word_, tag_range);
  }

  bool IsTaggedWith(TagType tag) const {
    return IsTaggedWithTagIn(TagRangeType(tag));
  }

  void SetMarkBit() { encoded_word_ |= kMarkBit; }

  void ClearMarkBit() { encoded_word_ &= ~kMarkBit; }

  bool HasMarkBitSet() const { return encoded_word_ & kMarkBit; }

  // Extracts the freelist pointer if this entry is a freelist entry.
  std::optional<uint32_t> ExtractFreelistLink() const {
    if (IsTaggedWith(kFreeEntryTag)) {
      return static_cast<uint32_t>((encoded_word_ & kPayloadMask) >>
                                   kPayloadShift);
    } else {
      return std::nullopt;
    }
  }

  void SetTag(TagType tag) {
    encoded_word_ =
        (encoded_word_ & ~kTagMask) | (static_cast<Address>(tag) << kTagShift);
  }

  TagType ExtractTag() const {
    return static_cast<TagType>((encoded_word_ & kTagMask) >> kTagShift);
  }

  bool ContainsFreelistLink() const { return IsTaggedWith(kFreeEntryTag); }

  bool ContainsEvacuationEntry() const {
    return IsTaggedWith(kEvacuationEntryTag);
  }

  Address ExtractEvacuationEntryHandleLocation() const {
    return Untag(kEvacuationEntryTag);
  }

  bool ContainsPointer() const {
    return !ContainsFreelistLink() && !ContainsEvacuationEntry();
  }

  bool operator==(TaggedPayload other) const {
    return encoded_word_ == other.encoded_word_;
  }

  bool operator!=(TaggedPayload other) const {
    return encoded_word_ != other.encoded_word_;
  }

 private:
  Address encoded_word_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TAGGED_PAYLOAD_H_
