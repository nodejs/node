// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRUSTED_POINTER_TAG_H_
#define V8_SANDBOX_TRUSTED_POINTER_TAG_H_

#include "src/base/strong-alias.h"
#include "src/common/globals.h"
#include "src/objects/instance-type.h"

namespace v8 {
namespace internal {

// Defines the list of valid trusted pointer tags.
//
// When accessing a trusted pointer, a TrustedPointerTag must be
// provided which indicates the expected instance type of the pointed-to
// object. When the sandbox is enabled, this tag is used to ensure type-safe
// access to objects referenced via trusted pointers: if the provided tag
// doesn't match the tag of the object in the trusted pointer table, an
// inaccessible pointer will be returned.
//
// A trusted pointer table entry has the following layout:
//
// On x64, the payload must be able to hold 57-bit addresses (LA57):
//
// +-----------+----------+----------------+
// | 7-bit tag | mark bit | 56-bit payload |
// +-----------+----------+----------------+
//
// On the other 64-bit architectures (e.g. Arm64), addresses are at most 48
// bits wide. The tag field is 15 bits wide there, but as tags are limited to
// 7 bits, its top 8 bits are currently unused and left for TBI/MTE:
//
// +---------------+-----------+----------+----------------+
// | 8 unused bits | 7-bit tag | mark bit | 48-bit payload |
// +---------------+-----------+----------+----------------+
//
// This format ensures that both the tag and the payload can be extracted
// efficiently, and thereby helps keep the performance overhead low.
#if V8_TARGET_ARCH_X64 && V8_SUPPORT_LA57
constexpr uint64_t kTrustedPointerTableTagMask = 0xfe00'0000'0000'0000;
constexpr uint64_t kTrustedPointerTableMarkBit = 0x0100'0000'0000'0000;
constexpr uint64_t kTrustedPointerTablePayloadMask = 0x00ff'ffff'ffff'ffff;
constexpr uint64_t kTrustedPointerTableTagShift = 57;
#else   // !(V8_TARGET_ARCH_X64 && V8_SUPPORT_LA57)
constexpr uint64_t kTrustedPointerTableTagMask = 0xfffe'0000'0000'0000;
constexpr uint64_t kTrustedPointerTableMarkBit = 0x0001'0000'0000'0000;
constexpr uint64_t kTrustedPointerTablePayloadMask = 0x0000'ffff'ffff'ffff;
constexpr uint64_t kTrustedPointerTableTagShift = 49;
#endif  // !(V8_TARGET_ARCH_X64 && V8_SUPPORT_LA57)
constexpr uint64_t kTrustedPointerTablePayloadShift = 0;

// The tag is limited to 7 bits since it needs to fit together with a marking
// bit into the unused parts of a pointer on x64 (see the layout above).
enum TrustedPointerTag : uint16_t {
  kTrustedPointerNullTag = 0,

  // Shared trusted pointers are owned by the shared Isolate and stored in the
  // shared trusted pointer table associated with that Isolate.
  kFirstSharedTrustedPointerTag,
  kSharedWasmTrustedInstanceDataTrustedPointerTag =
      kFirstSharedTrustedPointerTag,
  kSharedWasmDispatchTableTrustedPointerTag,
  kLastSharedTrustedPointerTag = kSharedWasmDispatchTableTrustedPointerTag,

  // Trusted pointers using these tags are kept in a per-Isolate trusted
  // pointer table and can only be accessed when this Isolate is active.
  kFirstPerIsolateTrustedPointerTag,
  kWasmInternalFunctionTrustedPointerTag = kFirstPerIsolateTrustedPointerTag,
  // Untagging performance matters for this tag, so it should be "fast".
  kWasmTrustedInstanceDataTrustedPointerTag,  // 4
  kWasmDispatchTableTrustedPointerTag,
  kWasmSuspenderTrustedPointerTag,
  kRegExpDataTrustedPointerTag,

  kFirstSFITrustedDataTag,
  // Untagging performance matters for this tag, so it should be "fast".
  kWasmExportedFunctionDataTrustedPointerTag = kFirstSFITrustedDataTag,  // 8
  kWasmCapiFunctionDataTrustedPointerTag,
  kInterpreterDataTrustedPointerTag,
  kUncompiledDataTrustedPointerTag,
  kBytecodeArrayTrustedPointerTag,
  // All code pointers share the same tag (pointing to Code objects). Their
  // instruction stream start is guarded by another tag (CodeEntrypointTag).
  kCodeTrustedPointerTag,
  kLastSFITrustedDataTag = kCodeTrustedPointerTag,
  kDebugInfoTrustedPointerTag,
  kLastPerIsolateTrustedPointerTag = kDebugInfoTrustedPointerTag,

  // The maximum tag in kAllTrustedPointerTags. Padded to a (pow2-1) to enable
  // fast, single-instruction bitwise untagging (see
  // IsFastTrustedPointerTagRange).
  kLastTrustedPointerFastTag = 0x0f,

  // Special tags.
  //
  // Currently we only use 7 bits (plus one marking bit), which is all that is
  // available on x64. On the other architectures we have spare bits in the
  // pointers if we ever need them (e.g. for something like MTE).
  //

  // A special tag for objects that should not (yet) be exposed to the sandbox.
  //
  // There are basically two ways to use this:
  //
  // 1. If the initialization of a group of related objects fails, and the
  //    individual objects aren't in a consistent state afterwards, then they
  //    can all be unpublished through a TrustedPointerPublishingScope.
  //
  // 2. If certain types of objects need to be validated before being
  //    accessible from within the sandbox (for example, bytecode arrays), then
  //    these objects can first be created in an unpublished state and then
  //    only be published after successful validation.
  kUnpublishedTrustedPointerTag = 0x7c,
  // Tag for zapped entries in the trusted pointer table.
  kTrustedPointerZappedEntryTag = 0x7d,
  // Not currently used for the trusted pointer table.
  kTrustedPointerEvacuationEntryTag = 0x7e,
  // Tag for free entries in the trusted pointer table.
  kTrustedPointerFreeEntryTag = 0x7f,
  kLastTrustedPointerTag = 0x7f,
};

// All tags must fit into the 7 tag bits of an x64 trusted pointer table entry.
static_assert(kLastTrustedPointerTag <= 0x7f);

using TrustedPointerTagRange = TagRange<TrustedPointerTag>;

#if V8_TARGET_ARCH_ARM64
// On ARM64 with TBI we must not have fast tags that set bits in the top byte
// as these will be ignored.
// 0x3f (63) has its highest bit at position 5. When shifted by 49 for untagging
// masks, it sets bits exactly up to 54, keeping bit 55 (the kernel address
// switch) and bits 56-63 (the TBI ignored byte) fully set. To prevent clearing
// bit 55 or higher, tags must not exceed 0x3f (63).
constexpr uint16_t kMaxFastTrustedPointerTagForARM64 = 0x3f;
#endif

// "Fast" tags are those that are powers of two. In that case, we can simply
// mask out the tag bit (and the marking bit) from the payload to untag the
// pointer. If the tags don't match, we'll be left with a non-canonical pointer
// which is guaranteed to crash on use. This is slightly more efficient than
// the generic tag-extract-and-compare approach.
V8_INLINE constexpr bool IsFastTrustedPointerTag(TrustedPointerTag tag) {
  DCHECK_NE(tag, kTrustedPointerNullTag);
#if V8_TARGET_ARCH_ARM64
  if (static_cast<uint16_t>(tag) > kMaxFastTrustedPointerTagForARM64) {
    return false;
  }
#endif
  return base::bits::IsPowerOfTwo(tag);
}

// Tag ranges are "fast" if they can be untagged with a single AND operation.
V8_INLINE constexpr bool IsFastTrustedPointerTagRange(
    TrustedPointerTagRange tag_range) {
  DCHECK_NE(tag_range.first, kTrustedPointerNullTag);
  // A tag range is "fast" (i.e. can be untagged via a single mask) if either:
  // 1. It contains a single tag which is "fast" (a power of two).
  // 2. It starts at 1 and ends at a power-of-two minus one.
  //
  // In both cases, the tag bits can be masked off with a single AND operation.
  // If the tag is not within the range, the resulting pointer will have at
  // least one bit set in the tag area and will therefore be non-canonical,
  // which is guaranteed to crash on use.
  if (tag_range.Size() == 1) {
    return IsFastTrustedPointerTag(tag_range.first);
  } else {
    uint16_t first = static_cast<uint16_t>(tag_range.first);
    uint16_t last = static_cast<uint16_t>(tag_range.last);
#if V8_TARGET_ARCH_ARM64
    if (last > kMaxFastTrustedPointerTagForARM64) return false;
#endif
    return first == 1 && base::bits::IsPowerOfTwo(last + 1);
  }
}

V8_INLINE constexpr uint64_t ComputeUntaggingMaskForFastTrustedPointerTag(
    TrustedPointerTagRange tag_range) {
  DCHECK(IsFastTrustedPointerTagRange(tag_range));
  return ~(
      (static_cast<uint64_t>(tag_range.last) << kTrustedPointerTableTagShift) |
      kTrustedPointerTableMarkBit);
}

constexpr TrustedPointerTagRange kAllSharedTrustedPointerTags(
    kFirstSharedTrustedPointerTag, kLastSharedTrustedPointerTag);
constexpr TrustedPointerTagRange kAllPerIsolateTrustedPointerTags(
    kFirstPerIsolateTrustedPointerTag, kLastPerIsolateTrustedPointerTag);
constexpr TrustedPointerTagRange kAllTrustedPointerTags(
    kFirstSharedTrustedPointerTag, kLastTrustedPointerFastTag);
constexpr TrustedPointerTagRange kAllTrustedPointerTagsIncludingUnpublished(
    kFirstSharedTrustedPointerTag, kUnpublishedTrustedPointerTag);

constexpr TrustedPointerTagRange kWasmFunctionDataTrustedPointerTagRange(
    kWasmExportedFunctionDataTrustedPointerTag,
    kWasmCapiFunctionDataTrustedPointerTag);

constexpr TrustedPointerTagRange kSFITrustedDataTrustedPointerRange(
    kFirstSFITrustedDataTag, kLastSFITrustedDataTag);

// The kAllTrustedPointerTags contains all regular tags including the code tag.
static_assert(kAllTrustedPointerTags.Contains(kAllSharedTrustedPointerTags));
static_assert(
    kAllTrustedPointerTags.Contains(kAllPerIsolateTrustedPointerTags));
static_assert(kAllTrustedPointerTags.Contains(kCodeTrustedPointerTag));

// None of the above must contain any special entries though.
static_assert(!kAllTrustedPointerTags.Contains(kUnpublishedTrustedPointerTag));

// The range is expected to be fast.
static_assert(IsFastTrustedPointerTagRange(kAllTrustedPointerTags));

// These are only included in kAllTrustedPointerTagsIncludingUnpublished.
static_assert(kAllTrustedPointerTagsIncludingUnpublished.Contains(
    kUnpublishedTrustedPointerTag));
static_assert(kAllTrustedPointerTagsIncludingUnpublished.Contains(
    kAllTrustedPointerTags));

// We expect certain tags to be "fast" as their untagging performance matters.
// See crbug.com/476810009 and crbug.com/562056770 for why these tags should be
// fast.
static_assert(
    IsFastTrustedPointerTag(kWasmTrustedInstanceDataTrustedPointerTag));
static_assert(
    IsFastTrustedPointerTag(kWasmExportedFunctionDataTrustedPointerTag));

V8_INLINE static constexpr bool IsSharedTrustedPointerType(
    TrustedPointerTag tag) {
  return kAllSharedTrustedPointerTags.Contains(tag);
}

V8_INLINE static constexpr bool IsSharedTrustedPointerType(
    TrustedPointerTagRange tag_range) {
  DCHECK(tag_range == kAllTrustedPointerTags ||
         IsSharedTrustedPointerType(tag_range.first) ==
             IsSharedTrustedPointerType(tag_range.last));
  return kAllSharedTrustedPointerTags.Contains(tag_range);
}

V8_INLINE static constexpr bool IsPerIsolateTrustedPointerType(
    TrustedPointerTag tag) {
  return kAllPerIsolateTrustedPointerTags.Contains(tag);
}

V8_INLINE constexpr bool IsValidTrustedPointerTag(TrustedPointerTag tag) {
  return kAllTrustedPointerTags.Contains(tag);
}

// Migrating objects into trusted space is typically performed in multiple
// steps, where all references to the object from inside the sandbox are first
// changed to trusted pointers before actually moving the object out of the
// sandbox. As we have CHECKs that trusted pointer table entries point outside
// of the sandbox, we need this helper function to disable that CHECK for
// objects that are in the process of being migrated into trusted space.
V8_INLINE constexpr bool IsTrustedSpaceMigrationInProgressForObjectsWithTag(
    TrustedPointerTag tag) {
  return false;
}

// TODO(saelo): Refactor TaggedPayload to not require this workaround.
V8_INLINE static constexpr bool ExternalPointerCanBeEmpty(
    TrustedPointerTagRange tag_range) {
  return false;
}

// The null tag is also considered an invalid tag since no trusted pointer
// field should be using this tag.
static_assert(!IsValidTrustedPointerTag(kTrustedPointerNullTag));

V8_INLINE TrustedPointerTag TrustedPointerTagFromInstanceType(
    InstanceType instance_type, SharedFlag shared) {
  switch (instance_type) {
    case CODE_TYPE:
      return kCodeTrustedPointerTag;
    case BYTECODE_ARRAY_TYPE:
      return kBytecodeArrayTrustedPointerTag;
    case INTERPRETER_DATA_TYPE:
      return kInterpreterDataTrustedPointerTag;
    case DEBUG_INFO_TYPE:
      return kDebugInfoTrustedPointerTag;
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE:
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE:
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE:
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE:
      // TODO(saelo): Consider adding support for inheritance hierarchies in
      // our tag checking mechanism.
      return kUncompiledDataTrustedPointerTag;
    case ATOM_REG_EXP_DATA_TYPE:
    case IR_REG_EXP_DATA_TYPE:
      // TODO(saelo): Consider adding support for inheritance hierarchies in
      // our tag checking mechanism.
      return kRegExpDataTrustedPointerTag;
#if V8_ENABLE_WEBASSEMBLY
    case WASM_DISPATCH_TABLE_TYPE:
      return shared ? kSharedWasmDispatchTableTrustedPointerTag
                    : kWasmDispatchTableTrustedPointerTag;
    case WASM_TRUSTED_INSTANCE_DATA_TYPE:
      return shared ? kSharedWasmTrustedInstanceDataTrustedPointerTag
                    : kWasmTrustedInstanceDataTrustedPointerTag;
    case WASM_INTERNAL_FUNCTION_TYPE:
      return kWasmInternalFunctionTrustedPointerTag;
    case WASM_SUSPENDER_OBJECT_TYPE:
      return kWasmSuspenderTrustedPointerTag;
    case WASM_FUNCTION_DATA_TYPE:
      // WasmFunctionData is effectively an abstract base class, so we shouldn't
      // have instances of this type.
      UNREACHABLE();
    case WASM_EXPORTED_FUNCTION_DATA_TYPE:
      return kWasmExportedFunctionDataTrustedPointerTag;
    case WASM_CAPI_FUNCTION_DATA_TYPE:
      return kWasmCapiFunctionDataTrustedPointerTag;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TRUSTED_POINTER_TAG_H_
