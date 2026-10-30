// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_DESERIALIZER_H_
#define V8_SNAPSHOT_SERIALIZER_DESERIALIZER_H_

#include <type_traits>

#include "src/codegen/external-reference-encoder.h"
#include "src/objects/visitors.h"
#include "src/snapshot/references.h"

namespace v8 {
namespace internal {

class Isolate;

// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer : public RootVisitor {
 public:
  static void IterateStartupObjectCache(Isolate* isolate, RootVisitor* visitor);

  static void IterateSharedHeapObjectCache(Isolate* isolate,
                                           RootVisitor* visitor);

 protected:
  enum class SlotType {
    kAnySlot,
    kMapSlot,
  };
  static bool CanBeDeferred(Tagged<HeapObject> o, SlotType slot_type);

  // clang-format off
#define UNUSED_SERIALIZER_BYTE_CODES(V)                           \
  /* Free range 0x28..0x2f */                                     \
  V(0x28) V(0x29) V(0x2a) V(0x2b) V(0x2c) V(0x2d) V(0x2e) V(0x2f)
  // clang-format on

  // The static assert below will trigger when the number of preallocated spaces
  // changed. If that happens, update the kNewObject and kBackref bytecode
  // ranges in the comments below.
  static_assert(4 == kNumberOfSnapshotSpaces);

  // First 32 root array items.
  static const int kRootArrayConstantsCount = 0x20;

  // 32 common raw data lengths.
  static const int kFixedRawDataCount = 0x20;
  // 16 repeats lengths.
  static const int kFixedRepeatRootCount = 0x10;

  // Top 128 back-referenced objects.
  static const int kFixedBackrefCount = 0x80;

  enum Bytecode : uint8_t {
    //
    // ---------- byte code range 0x00..0x27 ----------
    //

    // 0x00..0x03  Allocate new object, in specified space.
    kNewObject = 0x00,
    // Reference to previously allocated object.
    kBackref = 0x04,
    // Reference to an object in the read only heap.
    kReadOnlyHeapRef,
    // Object in the startup object cache.
    kStartupObjectCache,
    // Root array item.
    kRootArray,
    // Object provided in the attached list.
    kAttachedReference,
    // Object in the shared heap object cache.
    kSharedHeapObjectCache,
    // Do nothing, used for padding.
    kNop,
    // A tag emitted at strategic points in the snapshot to delineate sections.
    // If the deserializer does not find these at the expected moments then it
    // is an indication that the snapshot and the VM do not fit together.
    // Examine the build process for architecture, version or configuration
    // mismatches.
    kSynchronize,
    // Repeats of variable length of a root.
    kVariableRepeatRoot,
    // Used for embedder-allocated backing stores for TypedArrays.
    kOffHeapBackingStore,
    kOffHeapResizableBackingStore,
    // Used for embedder-provided serialization data for embedder fields.
    kEmbedderFieldsData,
    // Used for embedder-provided serialziation data for API wrappers.
    kApiWrapperFieldsData,
    // Raw data of variable length.
    kVariableRawData,
    // Used to encode external references provided through the API.
    kApiReference,
    // External reference referenced by id.
    kExternalReference,
    // Same as three bytecodes above but for serializing sandboxed external
    // pointer values.
    // TODO(v8:10391): Remove them once all ExternalPointer usages are
    // sandbox-ready.
    kSandboxedApiReference,
    kSandboxedExternalReference,
    kSandboxedRawExternalReference,
    // In-place weak references.
    kClearedWeakReference,
    kWeakPrefix,
    // Registers the current slot as a "pending" forward reference, to be later
    // filled by a corresponding resolution bytecode.
    kRegisterPendingForwardRef,
    // Resolves an existing "pending" forward reference to point to the current
    // object.
    kResolvePendingForwardRef,
    // Special construction bytecodes for the metamaps. In theory we could
    // reuse forward-references for this, but then the forward reference would
    // be registered during object map deserialization, before the object is
    // allocated, so there wouldn't be a allocated object whose map field we can
    // register as the pending field. We could either hack around this, or
    // simply introduce this new bytecode.
    kNewContextlessMetaMap,
    kNewContextfulMetaMap,
    // Extended maps store their actual size in bit_field_ex_, which would be
    // written after tagged fields without special handling. As a result
    // during deserialization of those tagged fields GC might happen and
    // observe an extended map with not-yet initialized size.
    // This bytecode must be serialized right after the map field.
    kExtendedMapBitFieldEx,
    // When the sandbox is enabled, a prefix indicating that the following
    // object is referenced through an indirect pointer, i.e. through an entry
    // in a pointer table.
    kTrustedPointerPrefix,
    // When the sandbox is enabled, this bytecode instructs the deserializer to
    // initialize the "self" indirect pointer of trusted objects, which
    // references the object's pointer table entry. As the "self" indirect
    // pointer is always the first field after the map word, it is guaranteed
    // that it will be deserialized before any inner objects, which may require
    // the pointer table entry for back reference to the trusted object.
    kInitializeSelfTrustedPointer,
    // This bytecode instructs the deserializer to allocate an entry in the
    // JSDispatchTable for the host object and store the corresponding dispatch
    // handle into the current slot.
    kAllocateJSDispatchEntry,
    // A back-reference to the already allocated n-th dispatch entry.
    kJSDispatchEntry,
    // A prefix indicating that the following object is referenced through a
    // protected pointer, i.e. a pointer from one trusted object to another.
    kProtectedPointerPrefix,
    // Reference to the n-th object exported by the trusted section.
    kTrustedExportRef,
    // Back reference to an object in the untrusted section.
    kUntrustedRef,
    // Reference to the n-th external reference in the trusted section.
    kTrustedExternalRef,
    // Reference to previously allocated object, also registering it in the
    // fixed backref table for subsequent 1-byte kFixedBackref references.
    kRegisterFixedBackref,
    // The host external string's resource, as an index into the API external
    // references. The deserializer stores a newly allocated cppgc wrapper for
    // it. Trusted sections only; an untrusted section refers to the resource
    // through the trusted section's table instead (kTrustedExternalRef).
    kExternalStringResource,

    //
    // ---------- byte code range 0x30..0x7f ----------
    //

    // 0x30..0x3f
    kFixedRepeatRoot = 0x30,

    // 0x40..0x5f
    kRootArrayConstants = 0x40,

    // 0x60..0x7f
    kFixedRawData = 0x60,

    //
    // ---------- byte code range 0x80..0xff ----------
    //

    // 0x80..0xff
    kFixedBackref = 0x80,
  };

  // Bytecodes that could corrupt memory outside the sandbox, which the
  // deserializer only accepts in trusted sections. There is no default case,
  // so that every new bytecode has to be classified here.
  static constexpr bool IsTrustedBytecode(Bytecode bytecode) {
    switch (bytecode) {
      case kApiReference:
      case kExternalReference:
      case kSandboxedApiReference:
      case kSandboxedExternalReference:
      case kSandboxedRawExternalReference:
      case kInitializeSelfTrustedPointer:
      case kAllocateJSDispatchEntry:
      case kProtectedPointerPrefix:
      case kExternalStringResource:
        return true;
      case kNewObject:
      case kBackref:
      case kReadOnlyHeapRef:
      case kStartupObjectCache:
      case kRootArray:
      case kAttachedReference:
      case kSharedHeapObjectCache:
      case kNop:
      case kSynchronize:
      case kVariableRepeatRoot:
      case kOffHeapBackingStore:
      case kOffHeapResizableBackingStore:
      case kEmbedderFieldsData:
      case kApiWrapperFieldsData:
      case kVariableRawData:
      case kClearedWeakReference:
      case kWeakPrefix:
      case kRegisterPendingForwardRef:
      case kResolvePendingForwardRef:
      case kNewContextlessMetaMap:
      case kNewContextfulMetaMap:
      case kExtendedMapBitFieldEx:
      case kTrustedPointerPrefix:
      case kJSDispatchEntry:
      case kTrustedExportRef:
      case kUntrustedRef:
      case kTrustedExternalRef:
      case kRegisterFixedBackref:
      case kRootArrayConstants:
      case kFixedRawData:
      case kFixedRepeatRoot:
      case kFixedBackref:
        return false;
    }
    // The rest of a bytecode range, or an unused bytecode.
    return false;
  }

  // Helper class for encoding and decoding a value into and from a bytecode.
  //
  // The value is encoded by allocating an entire bytecode range, and encoding
  // the value as an index in that range, starting at kMinValue; thus the range
  // of values
  //   [kMinValue, kMinValue + 1, ... , kMaxValue]
  // is encoded as
  //   [kBytecode, kBytecode + 1, ... , kBytecode + (N - 1)]
  // where N is the number of values, i.e. kMaxValue - kMinValue + 1.
  template <Bytecode kBytecode, int kMinValue, int kMaxValue,
            typename TValue = int>
  struct BytecodeValueEncoder {
    static_assert((kBytecode + kMaxValue - kMinValue) <= kMaxUInt8);

    static constexpr bool IsEncodable(TValue value) {
      return base::IsInRange(static_cast<int>(value), kMinValue, kMaxValue);
    }

    static constexpr uint8_t Encode(TValue value) {
      DCHECK(IsEncodable(value));
      return static_cast<uint8_t>(kBytecode + static_cast<int>(value) -
                                  kMinValue);
    }

    static constexpr TValue Decode(uint8_t bytecode) {
      DCHECK(base::IsInRange(bytecode, Encode(static_cast<TValue>(kMinValue)),
                             Encode(static_cast<TValue>(kMaxValue))));
      return static_cast<TValue>(bytecode - kBytecode + kMinValue);
    }
  };

  template <Bytecode bytecode>
  using SpaceEncoder =
      BytecodeValueEncoder<bytecode, 0, kNumberOfSnapshotSpaces - 1,
                           SnapshotSpace>;

  using NewObject = SpaceEncoder<kNewObject>;

  //
  // Some other constants.
  //

  // Sentinel after a new object to indicate that double alignment is needed.
  static const int kDoubleAlignmentSentinel = 0;

  // Raw data size encoding helpers.
  static const int kFirstEncodableFixedRawDataSize = 1;
  static const int kLastEncodableFixedRawDataSize =
      kFirstEncodableFixedRawDataSize + kFixedRawDataCount - 1;

  using FixedRawDataWithSize =
      BytecodeValueEncoder<kFixedRawData, kFirstEncodableFixedRawDataSize,
                           kLastEncodableFixedRawDataSize>;

  // Repeat count encoding helpers.
  static const int kFirstEncodableRepeatRootCount = 2;
  static const int kLastEncodableFixedRepeatRootCount =
      kFirstEncodableRepeatRootCount + kFixedRepeatRootCount - 1;
  static const int kFirstEncodableVariableRepeatRootCount =
      kLastEncodableFixedRepeatRootCount + 1;

  using FixedRepeatRootWithCount =
      BytecodeValueEncoder<kFixedRepeatRoot, kFirstEncodableRepeatRootCount,
                           kLastEncodableFixedRepeatRootCount>;

  // Encodes/decodes repeat count into a serialized variable repeat count
  // value.
  struct VariableRepeatRootCount {
    static constexpr bool IsEncodable(int repeat_count) {
      return repeat_count >= kFirstEncodableVariableRepeatRootCount;
    }

    static constexpr int Encode(int repeat_count) {
      DCHECK(IsEncodable(repeat_count));
      return repeat_count - kFirstEncodableVariableRepeatRootCount;
    }

    static constexpr int Decode(int value) {
      return value + kFirstEncodableVariableRepeatRootCount;
    }
  };

  using RootArrayConstant =
      BytecodeValueEncoder<kRootArrayConstants, 0, kRootArrayConstantsCount - 1,
                           RootIndex>;
  using FixedBackref =
      BytecodeValueEncoder<kFixedBackref, 0, kFixedBackrefCount - 1>;

  // This backing store reference value represents empty backing stores during
  // serialization/deserialization.
  static const uint32_t kEmptyBackingStoreRefSentinel = 0;

  // Number of bits used to encode the aligned chunk offset in kReadOnlyHeapRef.
  static constexpr int kReadOnlyHeapRefOffsetBits =
      kPageSizeBits - kObjectAlignmentBits;

  // An entry in the table of external references at the start of the trusted
  // section of a code cache, see kTrustedExternalRef.
  struct EncodedExternalReference {
    ExternalReferenceEncoder::Value reference;
    // kExternalPointerNullTag marks an external string resource, see
    // kExternalStringResource.
    // TODO(crbug.com/564863360): Use a proper CppHeapPointer tagging scheme.
    ExternalPointerTag tag;
    // Explicit padding, so that code caches are deterministic.
    uint16_t padding = 0;
  };
  static_assert(sizeof(EncodedExternalReference) == sizeof(uint64_t));
  static_assert(
      std::has_unique_object_representations_v<EncodedExternalReference>);
};

struct SerializeEmbedderFieldsCallback {
  explicit SerializeEmbedderFieldsCallback(
      v8::SerializeInternalFieldsCallback js_cb =
          v8::SerializeInternalFieldsCallback(),
      v8::SerializeContextDataCallback context_cb =
          v8::SerializeContextDataCallback(),
      v8::SerializeAPIWrapperCallback api_wrapper_cb =
          v8::SerializeAPIWrapperCallback())
      : js_object_callback(js_cb),
        context_callback(context_cb),
        api_wrapper_callback(api_wrapper_cb) {}
  v8::SerializeInternalFieldsCallback js_object_callback;
  v8::SerializeContextDataCallback context_callback;
  v8::SerializeAPIWrapperCallback api_wrapper_callback;
};

struct DeserializeEmbedderFieldsCallback {
  explicit DeserializeEmbedderFieldsCallback(
      v8::DeserializeInternalFieldsCallback js_cb =
          v8::DeserializeInternalFieldsCallback(),
      v8::DeserializeContextDataCallback context_cb =
          v8::DeserializeContextDataCallback(),
      v8::DeserializeAPIWrapperCallback api_wrapper_cb =
          v8::DeserializeAPIWrapperCallback())
      : js_object_callback(js_cb),
        context_callback(context_cb),
        api_wrapper_callback(api_wrapper_cb) {}
  v8::DeserializeInternalFieldsCallback js_object_callback;
  v8::DeserializeContextDataCallback context_callback;
  v8::DeserializeAPIWrapperCallback api_wrapper_callback;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_DESERIALIZER_H_
