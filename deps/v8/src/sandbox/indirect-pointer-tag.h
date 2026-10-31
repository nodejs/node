// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_TAG_H_
#define V8_SANDBOX_INDIRECT_POINTER_TAG_H_

#include "src/base/strong-alias.h"
#include "src/common/globals.h"
#include "src/objects/instance-type.h"

namespace v8 {
namespace internal {

// Defines the list of valid indirect pointer tags.
//
// When accessing a trusted/indirect pointer, an IndirectPointerTag must be
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
enum IndirectPointerTag : uint16_t {
  kIndirectPointerNullTag = 0,

  // Shared trusted pointers are owned by the shared Isolate and stored in the
  // shared trusted pointer table associated with that Isolate.
  kFirstSharedTrustedPointerTag,
  kSharedWasmTrustedInstanceDataIndirectPointerTag =
      kFirstSharedTrustedPointerTag,
  kSharedWasmDispatchTableIndirectPointerTag,
  kLastSharedTrustedPointerTag = kSharedWasmDispatchTableIndirectPointerTag,

  // Trusted pointers using these tags are kept in a per-Isolate trusted
  // pointer table and can only be accessed when this Isolate is active.
  kFirstPerIsolateTrustedPointerTag,
  kWasmInternalFunctionIndirectPointerTag = kFirstPerIsolateTrustedPointerTag,
  // Untagging performance matters for this tag, so it should be "fast".
  kWasmTrustedInstanceDataIndirectPointerTag,  // 4
  kWasmDispatchTableIndirectPointerTag,
  kWasmSuspenderIndirectPointerTag,
  kRegExpDataIndirectPointerTag,

  kFirstSFITrustedDataTag,
  // Untagging performance matters for this tag, so it should be "fast".
  kWasmExportedFunctionDataIndirectPointerTag = kFirstSFITrustedDataTag,  // 8
  kWasmCapiFunctionDataIndirectPointerTag,
  kInterpreterDataIndirectPointerTag,
  kUncompiledDataIndirectPointerTag,
  kBytecodeArrayIndirectPointerTag,
  // All code pointers share the same tag (pointing to Code objects). Their
  // instruction stream start is guarded by another tag (CodeEntrypointTag).
  kCodeIndirectPointerTag,
  kLastSFITrustedDataTag = kCodeIndirectPointerTag,
  kDebugInfoIndirectPointerTag,
  kLastPerIsolateTrustedPointerTag = kDebugInfoIndirectPointerTag,

  // The maximum tag in kAllIndirectPointerTags. Padded to a (pow2-1) to enable
  // fast, single-instruction bitwise untagging (see
  // IsFastIndirectPointerTagRange).
  kLastIndirectPointerFastTag = 0x0f,

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
  kUnpublishedIndirectPointerTag = 0x7c,
  // Tag for zapped entries in the trusted pointer table.
  kIndirectPointerZappedEntryTag = 0x7d,
  // Not currently used for the trusted pointer table.
  kIndirectPointerEvacuationEntryTag = 0x7e,
  // Tag for free entries in the trusted pointer table.
  kIndirectPointerFreeEntryTag = 0x7f,
  kLastIndirectPointerTag = 0x7f,
};

// All tags must fit into the 7 tag bits of an x64 trusted pointer table entry.
static_assert(kLastIndirectPointerTag <= 0x7f);

using IndirectPointerTagRange = TagRange<IndirectPointerTag>;

#if V8_TARGET_ARCH_ARM64
// On ARM64 with TBI we must not have fast tags that set bits in the top byte
// as these will be ignored.
// 0x3f (63) has its highest bit at position 5. When shifted by 49 for untagging
// masks, it sets bits exactly up to 54, keeping bit 55 (the kernel address
// switch) and bits 56-63 (the TBI ignored byte) fully set. To prevent clearing
// bit 55 or higher, tags must not exceed 0x3f (63).
constexpr uint16_t kMaxFastIndirectPointerTagForARM64 = 0x3f;
#endif

// "Fast" tags are those that are powers of two. In that case, we can simply
// mask out the tag bit (and the marking bit) from the payload to untag the
// pointer. If the tags don't match, we'll be left with a non-canonical pointer
// which is guaranteed to crash on use. This is slightly more efficient than
// the generic tag-extract-and-compare approach.
V8_INLINE constexpr bool IsFastIndirectPointerTag(IndirectPointerTag tag) {
  DCHECK_NE(tag, kIndirectPointerNullTag);
#if V8_TARGET_ARCH_ARM64
  if (static_cast<uint16_t>(tag) > kMaxFastIndirectPointerTagForARM64) {
    return false;
  }
#endif
  return base::bits::IsPowerOfTwo(tag);
}

// Tag ranges are "fast" if they can be untagged with a single AND operation.
V8_INLINE constexpr bool IsFastIndirectPointerTagRange(
    IndirectPointerTagRange tag_range) {
  DCHECK_NE(tag_range.first, kIndirectPointerNullTag);
  // A tag range is "fast" (i.e. can be untagged via a single mask) if either:
  // 1. It contains a single tag which is "fast" (a power of two).
  // 2. It starts at 1 and ends at a power-of-two minus one.
  //
  // In both cases, the tag bits can be masked off with a single AND operation.
  // If the tag is not within the range, the resulting pointer will have at
  // least one bit set in the tag area and will therefore be non-canonical,
  // which is guaranteed to crash on use.
  if (tag_range.Size() == 1) {
    return IsFastIndirectPointerTag(tag_range.first);
  } else {
    uint16_t first = static_cast<uint16_t>(tag_range.first);
    uint16_t last = static_cast<uint16_t>(tag_range.last);
#if V8_TARGET_ARCH_ARM64
    if (last > kMaxFastIndirectPointerTagForARM64) return false;
#endif
    return first == 1 && base::bits::IsPowerOfTwo(last + 1);
  }
}

V8_INLINE constexpr uint64_t ComputeUntaggingMaskForFastIndirectPointerTag(
    IndirectPointerTagRange tag_range) {
  DCHECK(IsFastIndirectPointerTagRange(tag_range));
  return ~(
      (static_cast<uint64_t>(tag_range.last) << kTrustedPointerTableTagShift) |
      kTrustedPointerTableMarkBit);
}

constexpr IndirectPointerTagRange kAllSharedIndirectPointerTags(
    kFirstSharedTrustedPointerTag, kLastSharedTrustedPointerTag);
constexpr IndirectPointerTagRange kAllPerIsolateIndirectPointerTags(
    kFirstPerIsolateTrustedPointerTag, kLastPerIsolateTrustedPointerTag);
constexpr IndirectPointerTagRange kAllIndirectPointerTags(
    kFirstSharedTrustedPointerTag, kLastIndirectPointerFastTag);
constexpr IndirectPointerTagRange kAllIndirectPointerTagsIncludingUnpublished(
    kFirstSharedTrustedPointerTag, kUnpublishedIndirectPointerTag);

constexpr IndirectPointerTagRange kWasmFunctionDataIndirectPointerTagRange(
    kWasmExportedFunctionDataIndirectPointerTag,
    kWasmCapiFunctionDataIndirectPointerTag);

constexpr IndirectPointerTagRange kSFITrustedDataIndirectPointerRange(
    kFirstSFITrustedDataTag, kLastSFITrustedDataTag);

// The kAllIndirectPointerTags contains all regular tags including the code tag.
static_assert(kAllIndirectPointerTags.Contains(kAllSharedIndirectPointerTags));
static_assert(
    kAllIndirectPointerTags.Contains(kAllPerIsolateIndirectPointerTags));
static_assert(kAllIndirectPointerTags.Contains(kCodeIndirectPointerTag));

// None of the above must contain any special entries though.
static_assert(
    !kAllIndirectPointerTags.Contains(kUnpublishedIndirectPointerTag));

// The range is expected to be fast.
static_assert(IsFastIndirectPointerTagRange(kAllIndirectPointerTags));

// These are only included in kAllIndirectPointerTagsIncludingUnpublished.
static_assert(kAllIndirectPointerTagsIncludingUnpublished.Contains(
    kUnpublishedIndirectPointerTag));
static_assert(kAllIndirectPointerTagsIncludingUnpublished.Contains(
    kAllIndirectPointerTags));

// We expect certain tags to be "fast" as their untagging performance matters.
// See crbug.com/476810009 and crbug.com/562056770 for why these tags should be
// fast.
static_assert(
    IsFastIndirectPointerTag(kWasmTrustedInstanceDataIndirectPointerTag));
static_assert(
    IsFastIndirectPointerTag(kWasmExportedFunctionDataIndirectPointerTag));

V8_INLINE static constexpr bool IsSharedTrustedPointerType(
    IndirectPointerTag tag) {
  return kAllSharedIndirectPointerTags.Contains(tag);
}

V8_INLINE static constexpr bool IsSharedTrustedPointerType(
    IndirectPointerTagRange tag_range) {
  DCHECK(tag_range == kAllIndirectPointerTags ||
         IsSharedTrustedPointerType(tag_range.first) ==
             IsSharedTrustedPointerType(tag_range.last));
  return kAllSharedIndirectPointerTags.Contains(tag_range);
}

V8_INLINE static constexpr bool IsPerIsolateTrustedPointerType(
    IndirectPointerTag tag) {
  return kAllPerIsolateIndirectPointerTags.Contains(tag);
}

V8_INLINE constexpr bool IsValidIndirectPointerTag(IndirectPointerTag tag) {
  return kAllIndirectPointerTags.Contains(tag);
}

// Migrating objects into trusted space is typically performed in multiple
// steps, where all references to the object from inside the sandbox are first
// changed to indirect pointers before actually moving the object out of the
// sandbox. As we have CHECKs that trusted pointer table entries point outside
// of the sandbox, we need this helper function to disable that CHECK for
// objects that are in the process of being migrated into trusted space.
V8_INLINE constexpr bool IsTrustedSpaceMigrationInProgressForObjectsWithTag(
    IndirectPointerTag tag) {
  return false;
}

// TODO(saelo): Refactor TaggedPayload to not require this workaround.
V8_INLINE static constexpr bool ExternalPointerCanBeEmpty(
    IndirectPointerTagRange tag_range) {
  return false;
}

// The null tag is also considered an invalid tag since no indirect pointer
// field should be using this tag.
static_assert(!IsValidIndirectPointerTag(kIndirectPointerNullTag));

V8_INLINE IndirectPointerTag IndirectPointerTagFromInstanceType(
    InstanceType instance_type, SharedFlag shared) {
  switch (instance_type) {
    case CODE_TYPE:
      return kCodeIndirectPointerTag;
    case BYTECODE_ARRAY_TYPE:
      return kBytecodeArrayIndirectPointerTag;
    case INTERPRETER_DATA_TYPE:
      return kInterpreterDataIndirectPointerTag;
    case DEBUG_INFO_TYPE:
      return kDebugInfoIndirectPointerTag;
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE:
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE:
    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_WITH_JOB_TYPE:
    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_AND_JOB_TYPE:
      // TODO(saelo): Consider adding support for inheritance hierarchies in
      // our tag checking mechanism.
      return kUncompiledDataIndirectPointerTag;
    case ATOM_REG_EXP_DATA_TYPE:
    case IR_REG_EXP_DATA_TYPE:
      // TODO(saelo): Consider adding support for inheritance hierarchies in
      // our tag checking mechanism.
      return kRegExpDataIndirectPointerTag;
#if V8_ENABLE_WEBASSEMBLY
    case WASM_DISPATCH_TABLE_TYPE:
      return shared ? kSharedWasmDispatchTableIndirectPointerTag
                    : kWasmDispatchTableIndirectPointerTag;
    case WASM_TRUSTED_INSTANCE_DATA_TYPE:
      return shared ? kSharedWasmTrustedInstanceDataIndirectPointerTag
                    : kWasmTrustedInstanceDataIndirectPointerTag;
    case WASM_INTERNAL_FUNCTION_TYPE:
      return kWasmInternalFunctionIndirectPointerTag;
    case WASM_SUSPENDER_OBJECT_TYPE:
      return kWasmSuspenderIndirectPointerTag;
    case WASM_FUNCTION_DATA_TYPE:
      // WasmFunctionData is effectively an abstract base class, so we shouldn't
      // have instances of this type.
      UNREACHABLE();
    case WASM_EXPORTED_FUNCTION_DATA_TYPE:
      return kWasmExportedFunctionDataIndirectPointerTag;
    case WASM_CAPI_FUNCTION_DATA_TYPE:
      return kWasmCapiFunctionDataIndirectPointerTag;
#endif  // V8_ENABLE_WEBASSEMBLY
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_INDIRECT_POINTER_TAG_H_
