// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_INL_H_
#define V8_OBJECTS_STRING_INL_H_

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/isolate-utils.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-table-inl.h"
#include "src/objects/string.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/external-pointer.h"
#include "src/strings/string-hasher-inl.h"
#include "src/strings/unicode-inl.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/string-tq-inl.inc"

class V8_NODISCARD SharedStringAccessGuardIfNeeded {
 public:
  // Creates no SharedMutexGuard<kShared> for the string access since it was
  // called from the main thread.
  explicit SharedStringAccessGuardIfNeeded(Isolate* isolate) {}

  // Creates a SharedMutexGuard<kShared> for the string access if it was called
  // from a background thread.
  explicit SharedStringAccessGuardIfNeeded(LocalIsolate* local_isolate) {
    if (IsNeeded(local_isolate)) {
      mutex_guard.emplace(local_isolate->internalized_string_access());
    }
  }

  // Slow version which gets the isolate from the String.
  explicit SharedStringAccessGuardIfNeeded(Tagged<String> str) {
    Isolate* isolate = GetIsolateIfNeeded(str);
    if (isolate != nullptr) {
      mutex_guard.emplace(isolate->internalized_string_access());
    }
  }

  static SharedStringAccessGuardIfNeeded NotNeeded() {
    return SharedStringAccessGuardIfNeeded();
  }

  static bool IsNeeded(Tagged<String> str, LocalIsolate* local_isolate) {
    return IsNeeded(local_isolate) && IsNeeded(str, false);
  }

  static bool IsNeeded(Tagged<String> str, bool check_local_heap = true) {
    if (check_local_heap) {
      LocalHeap* local_heap = LocalHeap::Current();
      if (!local_heap || local_heap->is_main_thread()) {
        // Don't acquire the lock for the main thread.
        return false;
      }
    }

    if (ReadOnlyHeap::Contains(str)) {
      // Don't acquire lock for strings in ReadOnlySpace.
      return false;
    }

    return true;
  }

  static bool IsNeeded(LocalIsolate* local_isolate) {
    // TODO(leszeks): Remove the nullptr check for local_isolate.
    return local_isolate && !local_isolate->heap()->is_main_thread();
  }

 private:
  // Default constructor and move constructor required for the NotNeeded()
  // static constructor.
  constexpr SharedStringAccessGuardIfNeeded() = default;
  constexpr SharedStringAccessGuardIfNeeded(SharedStringAccessGuardIfNeeded&&)
      V8_NOEXCEPT {
    DCHECK(!mutex_guard.has_value());
  }

  // Returns the Isolate from the String if we need it for the lock.
  static Isolate* GetIsolateIfNeeded(Tagged<String> str) {
    if (!IsNeeded(str)) return nullptr;

    Isolate* isolate;
    if (!GetIsolateFromHeapObject(str, &isolate)) {
      // If we can't get the isolate from the String, it must be read-only.
      DCHECK(ReadOnlyHeap::Contains(str));
      return nullptr;
    }
    return isolate;
  }

  base::Optional<base::SharedMutexGuard<base::kShared>> mutex_guard;
};

int String::length(AcquireLoadTag) const {
  return base::AsAtomic32::Acquire_Load(
      reinterpret_cast<const int32_t*>(field_address(kLengthOffset)));
}

void String::set_length(int value, ReleaseStoreTag) {
  base::AsAtomic32::Release_Store(
      reinterpret_cast<int32_t*>(field_address(kLengthOffset)), value);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(String)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqOneByteString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqTwoByteString)
TQ_OBJECT_CONSTRUCTORS_IMPL(InternalizedString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ConsString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ThinString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SlicedString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ExternalString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ExternalOneByteString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ExternalTwoByteString)

static_assert(kTaggedCanConvertToRawObjects);

StringShape::StringShape(const Tagged<String> str)
    : type_(str->map(kAcquireLoad)->instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(const Tagged<String> str, PtrComprCageBase cage_base)
    : type_(str->map(cage_base, kAcquireLoad)->instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(Tagged<Map> map) : type_(map->instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(InstanceType t) : type_(static_cast<uint32_t>(t)) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

bool StringShape::IsInternalized() const {
  DCHECK(valid());
  static_assert(kNotInternalizedTag != 0);
  return (type_ & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
}

bool StringShape::IsCons() const {
  return (type_ & kStringRepresentationMask) == kConsStringTag;
}

bool StringShape::IsThin() const {
  return (type_ & kStringRepresentationMask) == kThinStringTag;
}

bool StringShape::IsSliced() const {
  return (type_ & kStringRepresentationMask) == kSlicedStringTag;
}

bool StringShape::IsIndirect() const {
  return (type_ & kIsIndirectStringMask) == kIsIndirectStringTag;
}

bool StringShape::IsDirect() const { return !IsIndirect(); }

bool StringShape::IsExternal() const {
  return (type_ & kStringRepresentationMask) == kExternalStringTag;
}

bool StringShape::IsSequential() const {
  return (type_ & kStringRepresentationMask) == kSeqStringTag;
}

bool StringShape::IsUncachedExternal() const {
  return (type_ & kUncachedExternalStringMask) == kUncachedExternalStringTag;
}

bool StringShape::IsShared() const {
  // TODO(v8:12007): Set is_shared to true on internalized string when
  // v8_flags.shared_string_table is removed.
  return (type_ & kSharedStringMask) == kSharedStringTag ||
         (v8_flags.shared_string_table && IsInternalized());
}

StringRepresentationTag StringShape::representation_tag() const {
  uint32_t tag = (type_ & kStringRepresentationMask);
  return static_cast<StringRepresentationTag>(tag);
}

uint32_t StringShape::encoding_tag() const {
  return type_ & kStringEncodingMask;
}

uint32_t StringShape::representation_and_encoding_tag() const {
  return (type_ & (kStringRepresentationAndEncodingMask));
}

uint32_t StringShape::representation_encoding_and_shared_tag() const {
  return (type_ & (kStringRepresentationEncodingAndSharedMask));
}

static_assert((kStringRepresentationAndEncodingMask) ==
              Internals::kStringRepresentationAndEncodingMask);

static_assert(static_cast<uint32_t>(kStringEncodingMask) ==
              Internals::kStringEncodingMask);

bool StringShape::IsSequentialOneByte() const {
  return representation_and_encoding_tag() == kSeqOneByteStringTag;
}

bool StringShape::IsSequentialTwoByte() const {
  return representation_and_encoding_tag() == kSeqTwoByteStringTag;
}

bool StringShape::IsExternalOneByte() const {
  return representation_and_encoding_tag() == kExternalOneByteStringTag;
}

static_assert(kExternalOneByteStringTag ==
              Internals::kExternalOneByteRepresentationTag);

static_assert(v8::String::ONE_BYTE_ENCODING == kOneByteStringTag);

bool StringShape::IsExternalTwoByte() const {
  return representation_and_encoding_tag() == kExternalTwoByteStringTag;
}

static_assert(kExternalTwoByteStringTag ==
              Internals::kExternalTwoByteRepresentationTag);

static_assert(v8::String::TWO_BYTE_ENCODING == kTwoByteStringTag);

template <typename TDispatcher, typename TResult, typename... TArgs>
inline TResult StringShape::DispatchToSpecificTypeWithoutCast(TArgs&&... args) {
  switch (representation_and_encoding_tag()) {
    case kSeqStringTag | kOneByteStringTag:
      return TDispatcher::HandleSeqOneByteString(std::forward<TArgs>(args)...);
    case kSeqStringTag | kTwoByteStringTag:
      return TDispatcher::HandleSeqTwoByteString(std::forward<TArgs>(args)...);
    case kConsStringTag | kOneByteStringTag:
    case kConsStringTag | kTwoByteStringTag:
      return TDispatcher::HandleConsString(std::forward<TArgs>(args)...);
    case kExternalStringTag | kOneByteStringTag:
      return TDispatcher::HandleExternalOneByteString(
          std::forward<TArgs>(args)...);
    case kExternalStringTag | kTwoByteStringTag:
      return TDispatcher::HandleExternalTwoByteString(
          std::forward<TArgs>(args)...);
    case kSlicedStringTag | kOneByteStringTag:
    case kSlicedStringTag | kTwoByteStringTag:
      return TDispatcher::HandleSlicedString(std::forward<TArgs>(args)...);
    case kThinStringTag | kOneByteStringTag:
    case kThinStringTag | kTwoByteStringTag:
      return TDispatcher::HandleThinString(std::forward<TArgs>(args)...);
    default:
      return TDispatcher::HandleInvalidString(std::forward<TArgs>(args)...);
  }
}

// All concrete subclasses of String (leaves of the inheritance tree).
#define STRING_CLASS_TYPES(V) \
  V(SeqOneByteString)         \
  V(SeqTwoByteString)         \
  V(ConsString)               \
  V(ExternalOneByteString)    \
  V(ExternalTwoByteString)    \
  V(SlicedString)             \
  V(ThinString)

template <typename TDispatcher, typename TResult, typename... TArgs>
inline TResult StringShape::DispatchToSpecificType(Tagged<String> str,
                                                   TArgs&&... args) {
  class CastingDispatcher : public AllStatic {
   public:
#define DEFINE_METHOD(Type)                                         \
  static inline TResult Handle##Type(String str, TArgs&&... args) { \
    return TDispatcher::Handle##Type(Type::cast(str),               \
                                     std::forward<TArgs>(args)...); \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline TResult HandleInvalidString(Tagged<String> str,
                                              TArgs&&... args) {
      return TDispatcher::HandleInvalidString(str,
                                              std::forward<TArgs>(args)...);
    }
  };

  return DispatchToSpecificTypeWithoutCast<CastingDispatcher, TResult>(
      str, std::forward<TArgs>(args)...);
}

DEF_GETTER(String, IsOneByteRepresentation, bool) {
  uint32_t type = map(cage_base)->instance_type();
  return (type & kStringEncodingMask) == kOneByteStringTag;
}

DEF_GETTER(String, IsTwoByteRepresentation, bool) {
  uint32_t type = map(cage_base)->instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}

// static
bool String::IsOneByteRepresentationUnderneath(Tagged<String> string) {
  while (true) {
    uint32_t type = string->map()->instance_type();
    static_assert(kIsIndirectStringTag != 0);
    static_assert((kIsIndirectStringMask & kStringEncodingMask) == 0);
    DCHECK(string->IsFlat());
    switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
      case kOneByteStringTag:
        return true;
      case kTwoByteStringTag:
        return false;
      default:  // Cons, sliced, thin, strings need to go deeper.
        string = string->GetUnderlying();
    }
  }
}

base::uc32 FlatStringReader::Get(int index) const {
  if (is_one_byte_) {
    return Get<uint8_t>(index);
  } else {
    return Get<base::uc16>(index);
  }
}

template <typename Char>
Char FlatStringReader::Get(int index) const {
  DCHECK_EQ(is_one_byte_, sizeof(Char) == 1);
  DCHECK(0 <= index && index < length_);
  if (sizeof(Char) == 1) {
    return static_cast<Char>(static_cast<const uint8_t*>(start_)[index]);
  } else {
    return static_cast<Char>(static_cast<const base::uc16*>(start_)[index]);
  }
}

template <typename Char>
class SequentialStringKey final : public StringTableKey {
 public:
  SequentialStringKey(base::Vector<const Char> chars, uint64_t seed,
                      bool convert = false)
      : SequentialStringKey(StringHasher::HashSequentialString<Char>(
                                chars.begin(), chars.length(), seed),
                            chars, convert) {}

  SequentialStringKey(int raw_hash_field, base::Vector<const Char> chars,
                      bool convert = false)
      : StringTableKey(raw_hash_field, chars.length()),
        chars_(chars),
        convert_(convert) {}

  template <typename IsolateT>
  bool IsMatch(IsolateT* isolate, Tagged<String> s) {
    return s->IsEqualTo<String::EqualityType::kNoLengthCheck>(chars_, isolate);
  }

  template <typename IsolateT>
  void PrepareForInsertion(IsolateT* isolate) {
    if (sizeof(Char) == 1) {
      internalized_string_ = isolate->factory()->NewOneByteInternalizedString(
          base::Vector<const uint8_t>::cast(chars_), raw_hash_field());
    } else if (convert_) {
      internalized_string_ =
          isolate->factory()->NewOneByteInternalizedStringFromTwoByte(
              base::Vector<const uint16_t>::cast(chars_), raw_hash_field());
    } else {
      internalized_string_ = isolate->factory()->NewTwoByteInternalizedString(
          base::Vector<const uint16_t>::cast(chars_), raw_hash_field());
    }
  }

  Handle<String> GetHandleForInsertion() {
    DCHECK(!internalized_string_.is_null());
    return internalized_string_;
  }

 private:
  base::Vector<const Char> chars_;
  bool convert_;
  Handle<String> internalized_string_;
};

using OneByteStringKey = SequentialStringKey<uint8_t>;
using TwoByteStringKey = SequentialStringKey<uint16_t>;

template <typename SeqString>
class SeqSubStringKey final : public StringTableKey {
 public:
  using Char = typename SeqString::Char;
// VS 2017 on official builds gives this spurious warning:
// warning C4789: buffer 'key' of size 16 bytes will be overrun; 4 bytes will
// be written starting at offset 16
// https://bugs.chromium.org/p/v8/issues/detail?id=6068
#if defined(V8_CC_MSVC)
#pragma warning(push)
#pragma warning(disable : 4789)
#endif
  SeqSubStringKey(Isolate* isolate, Handle<SeqString> string, int from, int len,
                  bool convert = false)
      : StringTableKey(0, len),
        string_(string),
        from_(from),
        convert_(convert) {
    // We have to set the hash later.
    DisallowGarbageCollection no_gc;
    uint32_t raw_hash_field = StringHasher::HashSequentialString(
        string->GetChars(no_gc) + from, len, HashSeed(isolate));
    set_raw_hash_field(raw_hash_field);

    DCHECK_LE(0, length());
    DCHECK_LE(from_ + length(), string_->length());
    DCHECK_EQ(IsSeqOneByteString(*string_), sizeof(Char) == 1);
    DCHECK_EQ(IsSeqTwoByteString(*string_), sizeof(Char) == 2);
  }
#if defined(V8_CC_MSVC)
#pragma warning(pop)
#endif

  bool IsMatch(Isolate* isolate, Tagged<String> string) {
    DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(string));
    DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*string_));
    DisallowGarbageCollection no_gc;
    return string->IsEqualTo<String::EqualityType::kNoLengthCheck>(
        base::Vector<const Char>(string_->GetChars(no_gc) + from_, length()),
        isolate);
  }

  void PrepareForInsertion(Isolate* isolate) {
    if (sizeof(Char) == 1 || (sizeof(Char) == 2 && convert_)) {
      Handle<SeqOneByteString> result =
          isolate->factory()->AllocateRawOneByteInternalizedString(
              length(), raw_hash_field());
      DisallowGarbageCollection no_gc;
      CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
                length());
      internalized_string_ = result;
    } else {
      Handle<SeqTwoByteString> result =
          isolate->factory()->AllocateRawTwoByteInternalizedString(
              length(), raw_hash_field());
      DisallowGarbageCollection no_gc;
      CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
                length());
      internalized_string_ = result;
    }
  }

  Handle<String> GetHandleForInsertion() {
    DCHECK(!internalized_string_.is_null());
    return internalized_string_;
  }

 private:
  Handle<typename CharTraits<Char>::String> string_;
  int from_;
  bool convert_;
  Handle<String> internalized_string_;
};

using SeqOneByteSubStringKey = SeqSubStringKey<SeqOneByteString>;
using SeqTwoByteSubStringKey = SeqSubStringKey<SeqTwoByteString>;

bool String::Equals(Tagged<String> other) const {
  if (other == *this) return true;
  if (IsInternalizedString(*this) && IsInternalizedString(other)) {
    return false;
  }
  return SlowEquals(other);
}

// static
bool String::Equals(Isolate* isolate, Handle<String> one, Handle<String> two) {
  if (one.is_identical_to(two)) return true;
  if (IsInternalizedString(*one) && IsInternalizedString(*two)) {
    return false;
  }
  return SlowEquals(isolate, one, two);
}

template <String::EqualityType kEqType, typename Char>
bool String::IsEqualTo(base::Vector<const Char> str, Isolate* isolate) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return IsEqualToImpl<kEqType>(str, isolate,
                                SharedStringAccessGuardIfNeeded::NotNeeded());
}

template <String::EqualityType kEqType, typename Char>
bool String::IsEqualTo(base::Vector<const Char> str) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return IsEqualToImpl<kEqType>(str, GetPtrComprCageBase(*this),
                                SharedStringAccessGuardIfNeeded::NotNeeded());
}

template <String::EqualityType kEqType, typename Char>
bool String::IsEqualTo(base::Vector<const Char> str,
                       LocalIsolate* isolate) const {
  SharedStringAccessGuardIfNeeded access_guard(isolate);
  return IsEqualToImpl<kEqType>(str, isolate, access_guard);
}

template <String::EqualityType kEqType, typename Char>
bool String::IsEqualToImpl(
    base::Vector<const Char> str, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  size_t len = str.size();
  switch (kEqType) {
    case EqualityType::kWholeString:
      if (static_cast<size_t>(length()) != len) return false;
      break;
    case EqualityType::kPrefix:
      if (static_cast<size_t>(length()) < len) return false;
      break;
    case EqualityType::kNoLengthCheck:
      DCHECK_EQ(length(), len);
      break;
  }

  DisallowGarbageCollection no_gc;

  int slice_offset = 0;
  Tagged<String> string = *this;
  const Char* data = str.data();
  while (true) {
    int32_t type = string->map(cage_base)->instance_type();
    switch (type & kStringRepresentationAndEncodingMask) {
      case kSeqOneByteStringTag:
        return CompareCharsEqual(
            SeqOneByteString::cast(string)->GetChars(no_gc, access_guard) +
                slice_offset,
            data, len);
      case kSeqTwoByteStringTag:
        return CompareCharsEqual(
            SeqTwoByteString::cast(string)->GetChars(no_gc, access_guard) +
                slice_offset,
            data, len);
      case kExternalOneByteStringTag:
        return CompareCharsEqual(
            ExternalOneByteString::cast(string)->GetChars(cage_base) +
                slice_offset,
            data, len);
      case kExternalTwoByteStringTag:
        return CompareCharsEqual(
            ExternalTwoByteString::cast(string)->GetChars(cage_base) +
                slice_offset,
            data, len);

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        Tagged<SlicedString> slicedString = SlicedString::cast(string);
        slice_offset += slicedString->offset();
        string = slicedString->parent(cage_base);
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag: {
        // The ConsString path is more complex and rare, so call out to an
        // out-of-line handler.
        // Slices cannot refer to ConsStrings, so there cannot be a non-zero
        // slice offset here.
        DCHECK_EQ(slice_offset, 0);
        return IsConsStringEqualToImpl<Char>(ConsString::cast(string), str,
                                             cage_base, access_guard);
      }

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string)->actual(cage_base);
        continue;

      default:
        UNREACHABLE();
    }
  }
}

// static
template <typename Char>
bool String::IsConsStringEqualToImpl(
    Tagged<ConsString> string, base::Vector<const Char> str,
    PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  // Already checked the len in IsEqualToImpl. Check GE rather than EQ in case
  // this is a prefix check.
  DCHECK_GE(string->length(), str.size());

  ConsStringIterator iter(ConsString::cast(string));
  base::Vector<const Char> remaining_str = str;
  int offset;
  for (Tagged<String> segment = iter.Next(&offset); !segment.is_null();
       segment = iter.Next(&offset)) {
    // We create the iterator without an offset, so we should never have a
    // per-segment offset.
    DCHECK_EQ(offset, 0);
    // Compare the individual segment against the appropriate subvector of the
    // remaining string.
    size_t len = std::min<size_t>(segment->length(), remaining_str.size());
    base::Vector<const Char> sub_str = remaining_str.SubVector(0, len);
    if (!segment->IsEqualToImpl<EqualityType::kNoLengthCheck>(
            sub_str, cage_base, access_guard)) {
      return false;
    }
    remaining_str += len;
    if (remaining_str.empty()) break;
  }
  DCHECK_EQ(remaining_str.data(), str.end());
  DCHECK_EQ(remaining_str.size(), 0);
  return true;
}

bool String::IsOneByteEqualTo(base::Vector<const char> str) {
  return IsEqualTo(str);
}

template <typename Char>
const Char* String::GetDirectStringChars(
    PtrComprCageBase cage_base, const DisallowGarbageCollection& no_gc) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  DCHECK(StringShape(*this).IsDirect());
  return StringShape(*this, cage_base).IsExternal()
             ? CharTraits<Char>::ExternalString::cast(*this).GetChars(cage_base)
             : CharTraits<Char>::String::cast(*this).GetChars(no_gc);
}

template <typename Char>
const Char* String::GetDirectStringChars(
    PtrComprCageBase cage_base, const DisallowGarbageCollection& no_gc,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  DCHECK(StringShape(*this).IsDirect());
  return StringShape(*this, cage_base).IsExternal()
             ? CharTraits<Char>::ExternalString::cast(*this)->GetChars(
                   cage_base)
             : CharTraits<Char>::String::cast(*this)->GetChars(no_gc,
                                                               access_guard);
}

// static
Handle<String> String::Flatten(Isolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  DisallowGarbageCollection no_gc;  // Unhandlified code.
  PtrComprCageBase cage_base(isolate);
  Tagged<String> s = *string;
  StringShape shape(s, cage_base);

  // Shortcut already-flat strings.
  if (V8_LIKELY(shape.IsDirect())) return string;

  if (shape.IsCons()) {
    DCHECK(!Object::InSharedHeap(s));
    Tagged<ConsString> cons = ConsString::cast(s);
    if (!cons->IsFlat(isolate)) {
      AllowGarbageCollection yes_gc;
      return SlowFlatten(isolate, handle(cons, isolate), allocation);
    }
    s = cons->first(cage_base);
    shape = StringShape(s, cage_base);
  }

  if (shape.IsThin()) {
    s = ThinString::cast(s)->actual(cage_base);
    DCHECK(!IsConsString(s));
  }

  return handle(s, isolate);
}

// static
Handle<String> String::Flatten(LocalIsolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  // We should never pass non-flat strings to String::Flatten when off-thread.
  DCHECK(string->IsFlat());
  return string;
}

// static
base::Optional<String::FlatContent> String::TryGetFlatContentFromDirectString(
    PtrComprCageBase cage_base, const DisallowGarbageCollection& no_gc,
    Tagged<String> string, int offset, int length,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(length, 0);
  DCHECK_LE(offset + length, string->length());
  switch (StringShape{string, cage_base}.representation_and_encoding_tag()) {
    case kSeqOneByteStringTag:
      return FlatContent(
          SeqOneByteString::cast(string)->GetChars(no_gc, access_guard) +
              offset,
          length, no_gc);
    case kSeqTwoByteStringTag:
      return FlatContent(
          SeqTwoByteString::cast(string)->GetChars(no_gc, access_guard) +
              offset,
          length, no_gc);
    case kExternalOneByteStringTag:
      return FlatContent(
          ExternalOneByteString::cast(string)->GetChars(cage_base) + offset,
          length, no_gc);
    case kExternalTwoByteStringTag:
      return FlatContent(
          ExternalTwoByteString::cast(string)->GetChars(cage_base) + offset,
          length, no_gc);
    default:
      return {};
  }
  UNREACHABLE();
}

String::FlatContent String::GetFlatContent(
    const DisallowGarbageCollection& no_gc) {
  return GetFlatContent(no_gc, SharedStringAccessGuardIfNeeded::NotNeeded());
}

String::FlatContent::FlatContent(const uint8_t* start, int length,
                                 const DisallowGarbageCollection& no_gc)
    : onebyte_start(start), length_(length), state_(ONE_BYTE), no_gc_(no_gc) {
#ifdef ENABLE_SLOW_DCHECKS
  checksum_ = ComputeChecksum();
#endif
}

String::FlatContent::FlatContent(const base::uc16* start, int length,
                                 const DisallowGarbageCollection& no_gc)
    : twobyte_start(start), length_(length), state_(TWO_BYTE), no_gc_(no_gc) {
#ifdef ENABLE_SLOW_DCHECKS
  checksum_ = ComputeChecksum();
#endif
}

String::FlatContent::~FlatContent() {
  // When ENABLE_SLOW_DCHECKS, check the string contents did not change during
  // the lifetime of the FlatContent. To avoid extra memory use, only the hash
  // is checked instead of snapshotting the full character data.
  //
  // If you crashed here, it means something changed the character data of this
  // FlatContent during its lifetime (e.g. GC relocated the string). This is
  // almost always a bug. If you are certain it is not a bug, you can disable
  // the checksum verification in the caller by calling
  // UnsafeDisableChecksumVerification().
  SLOW_DCHECK(checksum_ == kChecksumVerificationDisabled ||
              checksum_ == ComputeChecksum());
}

#ifdef ENABLE_SLOW_DCHECKS
uint32_t String::FlatContent::ComputeChecksum() const {
  constexpr uint64_t hashseed = 1;
  uint32_t hash;
  if (state_ == ONE_BYTE) {
    hash = StringHasher::HashSequentialString(onebyte_start, length_, hashseed);
  } else {
    DCHECK_EQ(TWO_BYTE, state_);
    hash = StringHasher::HashSequentialString(twobyte_start, length_, hashseed);
  }
  DCHECK_NE(kChecksumVerificationDisabled, hash);
  return hash;
}
#endif

String::FlatContent String::GetFlatContent(
    const DisallowGarbageCollection& no_gc,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  base::Optional<FlatContent> flat_content = TryGetFlatContentFromDirectString(
      cage_base, no_gc, *this, 0, length(), access_guard);
  if (flat_content.has_value()) return flat_content.value();
  return SlowGetFlatContent(no_gc, access_guard);
}

Handle<String> String::Share(Isolate* isolate, Handle<String> string) {
  DCHECK(v8_flags.shared_string_table);
  MaybeHandle<Map> new_map;
  switch (
      isolate->factory()->ComputeSharingStrategyForString(string, &new_map)) {
    case StringTransitionStrategy::kCopy:
      return SlowShare(isolate, string);
    case StringTransitionStrategy::kInPlace:
      // A relaxed write is sufficient here, because at this point the string
      // has not yet escaped the current thread.
      DCHECK(Object::InSharedHeap(*string));
      string->set_map_no_write_barrier(*new_map.ToHandleChecked());
      return string;
    case StringTransitionStrategy::kAlreadyTransitioned:
      return string;
  }
}

uint16_t String::Get(int index) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return GetImpl(index, GetPtrComprCageBase(*this),
                 SharedStringAccessGuardIfNeeded::NotNeeded());
}

uint16_t String::Get(int index, Isolate* isolate) const {
  SharedStringAccessGuardIfNeeded scope(isolate);
  return GetImpl(index, isolate, scope);
}

uint16_t String::Get(int index, LocalIsolate* local_isolate) const {
  SharedStringAccessGuardIfNeeded scope(local_isolate);
  return GetImpl(index, local_isolate, scope);
}

uint16_t String::Get(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  return GetImpl(index, cage_base, access_guard);
}

uint16_t String::GetImpl(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  DCHECK(index >= 0 && index < length());

  class StringGetDispatcher : public AllStatic {
   public:
#define DEFINE_METHOD(Type)                                  \
  static inline uint16_t Handle##Type(                       \
      Type str, int index, PtrComprCageBase cage_base,       \
      const SharedStringAccessGuardIfNeeded& access_guard) { \
    return str->Get(index, cage_base, access_guard);         \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline uint16_t HandleInvalidString(
        Tagged<String> str, int index, PtrComprCageBase cage_base,
        const SharedStringAccessGuardIfNeeded& access_guard) {
      UNREACHABLE();
    }
  };

  return StringShape(Tagged<String>(*this))
      .DispatchToSpecificType<StringGetDispatcher, uint16_t>(
          *this, index, cage_base, access_guard);
}

void String::Set(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  DCHECK(StringShape(*this).IsSequential());

  return IsOneByteRepresentation()
             ? SeqOneByteString::cast(*this)->SeqOneByteStringSet(index, value)
             : SeqTwoByteString::cast(*this)->SeqTwoByteStringSet(index, value);
}

bool String::IsFlat() const { return IsFlat(GetPtrComprCageBase(*this)); }

bool String::IsFlat(PtrComprCageBase cage_base) const {
  if (!StringShape(*this, cage_base).IsCons()) return true;
  return ConsString::cast(*this)->IsFlat(cage_base);
}

bool String::IsShared() const { return IsShared(GetPtrComprCageBase(*this)); }

bool String::IsShared(PtrComprCageBase cage_base) const {
  const bool result = StringShape(*this, cage_base).IsShared();
  DCHECK_IMPLIES(result, Object::InSharedHeap(*this));
  return result;
}

Tagged<String> String::GetUnderlying() const {
  // Giving direct access to underlying string only makes sense if the
  // wrapping string is already flattened.
  DCHECK(IsFlat());
  DCHECK(StringShape(*this).IsIndirect());
  static_assert(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(SlicedString::kParentOffset));
  static_assert(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(ThinString::kActualOffset));
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return TaggedField<String, kUnderlyingOffset>::load(*this);
}

template <class Visitor>
Tagged<ConsString> String::VisitFlat(Visitor* visitor, Tagged<String> string,
                                     const int offset) {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(string));
  return VisitFlat(visitor, string, offset,
                   SharedStringAccessGuardIfNeeded::NotNeeded());
}

template <class Visitor>
Tagged<ConsString> String::VisitFlat(
    Visitor* visitor, Tagged<String> string, const int offset,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  DisallowGarbageCollection no_gc;
  int slice_offset = offset;
  const int length = string->length();
  DCHECK(offset <= length);
  PtrComprCageBase cage_base = GetPtrComprCageBase(string);
  while (true) {
    int32_t tag =
        StringShape(string, cage_base).representation_and_encoding_tag();
    switch (tag) {
      case kSeqOneByteStringTag:
        visitor->VisitOneByteString(
            SeqOneByteString::cast(string)->GetChars(no_gc, access_guard) +
                slice_offset,
            length - offset);
        return ConsString();

      case kSeqTwoByteStringTag:
        visitor->VisitTwoByteString(
            SeqTwoByteString::cast(string)->GetChars(no_gc, access_guard) +
                slice_offset,
            length - offset);
        return ConsString();

      case kExternalOneByteStringTag:
        visitor->VisitOneByteString(
            ExternalOneByteString::cast(string)->GetChars(cage_base) +
                slice_offset,
            length - offset);
        return ConsString();

      case kExternalTwoByteStringTag:
        visitor->VisitTwoByteString(
            ExternalTwoByteString::cast(string)->GetChars(cage_base) +
                slice_offset,
            length - offset);
        return ConsString();

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        Tagged<SlicedString> slicedString = SlicedString::cast(string);
        slice_offset += slicedString->offset();
        string = slicedString->parent(cage_base);
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag:
        return ConsString::cast(string);

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string)->actual(cage_base);
        continue;

      default:
        UNREACHABLE();
    }
  }
}

bool String::IsWellFormedUnicode(Isolate* isolate, Handle<String> string) {
  // One-byte strings are definitionally well formed and cannot have unpaired
  // surrogates.
  if (string->IsOneByteRepresentation()) return true;

  // TODO(v8:13557): The two-byte case can be optimized by extending the
  // InstanceType. See
  // https://docs.google.com/document/d/15f-1c_Ysw3lvjy_Gx0SmmD9qeO8UuXuAbWIpWCnTDO8/
  string = Flatten(isolate, string);
  if (String::IsOneByteRepresentationUnderneath(*string)) return true;
  DisallowGarbageCollection no_gc;
  String::FlatContent flat = string->GetFlatContent(no_gc);
  DCHECK(flat.IsFlat());
  const uint16_t* data = flat.ToUC16Vector().begin();
  return !unibrow::Utf16::HasUnpairedSurrogate(data, string->length());
}

template <>
inline base::Vector<const uint8_t> String::GetCharVector(
    const DisallowGarbageCollection& no_gc) {
  String::FlatContent flat = GetFlatContent(no_gc);
  DCHECK(flat.IsOneByte());
  return flat.ToOneByteVector();
}

template <>
inline base::Vector<const base::uc16> String::GetCharVector(
    const DisallowGarbageCollection& no_gc) {
  String::FlatContent flat = GetFlatContent(no_gc);
  DCHECK(flat.IsTwoByte());
  return flat.ToUC16Vector();
}

uint8_t SeqOneByteString::Get(int index) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return Get(index, GetPtrComprCageBase(*this),
             SharedStringAccessGuardIfNeeded::NotNeeded());
}

uint8_t SeqOneByteString::Get(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return ReadField<uint8_t>(kHeaderSize + index * kCharSize);
}

void SeqOneByteString::SeqOneByteStringSet(int index, uint16_t value) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, length());
  DCHECK_LE(value, kMaxOneByteCharCode);
  WriteField<uint8_t>(kHeaderSize + index * kCharSize,
                      static_cast<uint8_t>(value));
}

void SeqOneByteString::SeqOneByteStringSetChars(int index,
                                                const uint8_t* string,
                                                int string_length) {
  DCHECK_LE(0, index);
  DCHECK_LT(index + string_length, length());
  void* address =
      reinterpret_cast<void*>(field_address(kHeaderSize + index * kCharSize));
  memcpy(address, string, string_length);
}

Address SeqOneByteString::GetCharsAddress() const {
  return field_address(kHeaderSize);
}

uint8_t* SeqOneByteString::GetChars(
    const DisallowGarbageCollection& no_gc) const {
  USE(no_gc);
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return reinterpret_cast<uint8_t*>(GetCharsAddress());
}

uint8_t* SeqOneByteString::GetChars(
    const DisallowGarbageCollection& no_gc,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(no_gc);
  USE(access_guard);
  return reinterpret_cast<uint8_t*>(GetCharsAddress());
}

Address SeqTwoByteString::GetCharsAddress() const {
  return field_address(kHeaderSize);
}

base::uc16* SeqTwoByteString::GetChars(
    const DisallowGarbageCollection& no_gc) const {
  USE(no_gc);
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return reinterpret_cast<base::uc16*>(GetCharsAddress());
}

base::uc16* SeqTwoByteString::GetChars(
    const DisallowGarbageCollection& no_gc,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(no_gc);
  USE(access_guard);
  return reinterpret_cast<base::uc16*>(GetCharsAddress());
}

uint16_t SeqTwoByteString::Get(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return ReadField<uint16_t>(kHeaderSize + index * kShortSize);
}

void SeqTwoByteString::SeqTwoByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  WriteField<uint16_t>(kHeaderSize + index * kShortSize, value);
}

// Due to ThinString rewriting, concurrent visitors need to read the length with
// acquire semantics.
inline int SeqOneByteString::AllocatedSize() {
  return SizeFor(length(kAcquireLoad));
}
inline int SeqTwoByteString::AllocatedSize() {
  return SizeFor(length(kAcquireLoad));
}

// static
bool SeqOneByteString::IsCompatibleMap(Tagged<Map> map, ReadOnlyRoots roots) {
  return map == roots.seq_one_byte_string_map() ||
         map == roots.shared_seq_one_byte_string_map();
}

// static
bool SeqTwoByteString::IsCompatibleMap(Tagged<Map> map, ReadOnlyRoots roots) {
  return map == roots.seq_two_byte_string_map() ||
         map == roots.shared_seq_two_byte_string_map();
}

void SlicedString::set_parent(Tagged<String> parent, WriteBarrierMode mode) {
  DCHECK(IsSeqString(parent) || IsExternalString(parent));
  TorqueGeneratedSlicedString<SlicedString, Super>::set_parent(parent, mode);
}

Tagged<Object> ConsString::unchecked_first() const {
  return TaggedField<Object, kFirstOffset>::load(*this);
}

Tagged<Object> ConsString::unchecked_second() const {
  return RELAXED_READ_FIELD(*this, kSecondOffset);
}

bool ConsString::IsFlat(PtrComprCageBase cage_base) const {
  return second(cage_base)->length() == 0;
}

DEF_GETTER(ThinString, unchecked_actual, Tagged<HeapObject>) {
  return TaggedField<HeapObject, kActualOffset>::load(cage_base, *this);
}

bool ExternalString::is_uncached() const {
  InstanceType type = map()->instance_type();
  return (type & kUncachedExternalStringMask) == kUncachedExternalStringTag;
}

void ExternalString::InitExternalPointerFields(Isolate* isolate) {
  InitExternalPointerField<kExternalStringResourceTag>(kResourceOffset, isolate,
                                                       kNullAddress);
  if (is_uncached()) return;
  InitExternalPointerField<kExternalStringResourceDataTag>(
      kResourceDataOffset, isolate, kNullAddress);
}

void ExternalString::VisitExternalPointers(ObjectVisitor* visitor) const {
  visitor->VisitExternalPointer(*this, RawExternalPointerField(kResourceOffset),
                                kExternalStringResourceTag);
  if (is_uncached()) return;
  visitor->VisitExternalPointer(*this,
                                RawExternalPointerField(kResourceDataOffset),
                                kExternalStringResourceDataTag);
}

DEF_GETTER(ExternalString, resource_as_address, Address) {
  Isolate* isolate = GetIsolateForSandbox(*this);
  return ReadExternalPointerField<kExternalStringResourceTag>(kResourceOffset,
                                                              isolate);
}

void ExternalString::set_address_as_resource(Isolate* isolate, Address value) {
  WriteExternalPointerField<kExternalStringResourceTag>(kResourceOffset,
                                                        isolate, value);
  if (IsExternalOneByteString(*this)) {
    ExternalOneByteString::cast(*this)->update_data_cache(isolate);
  } else {
    ExternalTwoByteString::cast(*this)->update_data_cache(isolate);
  }
}

uint32_t ExternalString::GetResourceRefForDeserialization() {
  ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  return static_cast<uint32_t>(encoded_address);
}

void ExternalString::SetResourceRefForSerialization(uint32_t ref) {
  WriteField<ExternalPointer_t>(kResourceOffset,
                                static_cast<ExternalPointer_t>(ref));
  if (is_uncached()) return;
  WriteField<ExternalPointer_t>(kResourceDataOffset, kNullExternalPointer);
}

void ExternalString::DisposeResource(Isolate* isolate) {
  Address value = ReadExternalPointerField<kExternalStringResourceTag>(
      kResourceOffset, isolate);
  v8::String::ExternalStringResourceBase* resource =
      reinterpret_cast<v8::String::ExternalStringResourceBase*>(value);

  // Dispose of the C++ object if it has not already been disposed.
  if (resource != nullptr) {
    resource->Dispose();
    WriteExternalPointerField<kExternalStringResourceTag>(
        kResourceOffset, isolate, kNullAddress);
  }
}

DEF_GETTER(ExternalOneByteString, resource,
           const ExternalOneByteString::Resource*) {
  return mutable_resource();
}

DEF_GETTER(ExternalOneByteString, mutable_resource,
           ExternalOneByteString::Resource*) {
  return reinterpret_cast<Resource*>(resource_as_address(cage_base));
}

void ExternalOneByteString::update_data_cache(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  if (is_uncached()) {
    if (resource(isolate)->IsCacheable())
      mutable_resource(isolate)->UpdateDataCache();
  } else {
    WriteExternalPointerField<kExternalStringResourceDataTag>(
        kResourceDataOffset, isolate,
        reinterpret_cast<Address>(resource(isolate)->data()));
  }
}

void ExternalOneByteString::SetResource(
    Isolate* isolate, const ExternalOneByteString::Resource* resource) {
  set_resource(isolate, resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length();
  if (new_payload > 0) {
    isolate->heap()->UpdateExternalString(*this, 0, new_payload);
  }
}

void ExternalOneByteString::set_resource(
    Isolate* isolate, const ExternalOneByteString::Resource* resource) {
  WriteExternalPointerField<kExternalStringResourceTag>(
      kResourceOffset, isolate, reinterpret_cast<Address>(resource));
  if (resource != nullptr) update_data_cache(isolate);
}

const uint8_t* ExternalOneByteString::GetChars(
    PtrComprCageBase cage_base) const {
  DisallowGarbageCollection no_gc;
  auto res = resource(cage_base);
  if (is_uncached()) {
    if (res->IsCacheable()) {
      // TODO(solanes): Teach TurboFan/CSA to not bailout to the runtime to
      // avoid this call.
      return reinterpret_cast<const uint8_t*>(res->cached_data());
    }
#if DEBUG
    // Check that this method is called only from the main thread if we have an
    // uncached string with an uncacheable resource.
    {
      Isolate* isolate;
      DCHECK_IMPLIES(GetIsolateFromHeapObject(*this, &isolate),
                     ThreadId::Current() == isolate->thread_id());
    }
#endif
  }

  return reinterpret_cast<const uint8_t*>(res->data());
}

uint8_t ExternalOneByteString::Get(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return GetChars(cage_base)[index];
}

DEF_GETTER(ExternalTwoByteString, resource,
           const ExternalTwoByteString::Resource*) {
  return mutable_resource();
}

DEF_GETTER(ExternalTwoByteString, mutable_resource,
           ExternalTwoByteString::Resource*) {
  return reinterpret_cast<Resource*>(resource_as_address(cage_base));
}

void ExternalTwoByteString::update_data_cache(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  if (is_uncached()) {
    if (resource(isolate)->IsCacheable())
      mutable_resource(isolate)->UpdateDataCache();
  } else {
    WriteExternalPointerField<kExternalStringResourceDataTag>(
        kResourceDataOffset, isolate,
        reinterpret_cast<Address>(resource(isolate)->data()));
  }
}

void ExternalTwoByteString::SetResource(
    Isolate* isolate, const ExternalTwoByteString::Resource* resource) {
  set_resource(isolate, resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length() * 2;
  if (new_payload > 0) {
    isolate->heap()->UpdateExternalString(*this, 0, new_payload);
  }
}

void ExternalTwoByteString::set_resource(
    Isolate* isolate, const ExternalTwoByteString::Resource* resource) {
  WriteExternalPointerField<kExternalStringResourceTag>(
      kResourceOffset, isolate, reinterpret_cast<Address>(resource));
  if (resource != nullptr) update_data_cache(isolate);
}

const uint16_t* ExternalTwoByteString::GetChars(
    PtrComprCageBase cage_base) const {
  DisallowGarbageCollection no_gc;
  auto res = resource(cage_base);
  if (is_uncached()) {
    if (res->IsCacheable()) {
      // TODO(solanes): Teach TurboFan/CSA to not bailout to the runtime to
      // avoid this call.
      return res->cached_data();
    }
#if DEBUG
    // Check that this method is called only from the main thread if we have an
    // uncached string with an uncacheable resource.
    {
      Isolate* isolate;
      DCHECK_IMPLIES(GetIsolateFromHeapObject(*this, &isolate),
                     ThreadId::Current() == isolate->thread_id());
    }
#endif
  }

  return res->data();
}

uint16_t ExternalTwoByteString::Get(
    int index, PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return GetChars(cage_base)[index];
}

const uint16_t* ExternalTwoByteString::ExternalTwoByteStringGetData(
    unsigned start) {
  return GetChars(GetPtrComprCageBase(*this)) + start;
}

int ConsStringIterator::OffsetForDepth(int depth) { return depth & kDepthMask; }

void ConsStringIterator::PushLeft(Tagged<ConsString> string) {
  frames_[depth_++ & kDepthMask] = string;
}

void ConsStringIterator::PushRight(Tagged<ConsString> string) {
  // Inplace update.
  frames_[(depth_ - 1) & kDepthMask] = string;
}

void ConsStringIterator::AdjustMaximumDepth() {
  if (depth_ > maximum_depth_) maximum_depth_ = depth_;
}

void ConsStringIterator::Pop() {
  DCHECK_GT(depth_, 0);
  DCHECK(depth_ <= maximum_depth_);
  depth_--;
}

class StringCharacterStream {
 public:
  inline explicit StringCharacterStream(Tagged<String> string, int offset = 0);
  StringCharacterStream(const StringCharacterStream&) = delete;
  StringCharacterStream& operator=(const StringCharacterStream&) = delete;
  inline uint16_t GetNext();
  inline bool HasMore();
  inline void Reset(Tagged<String> string, int offset = 0);
  inline void VisitOneByteString(const uint8_t* chars, int length);
  inline void VisitTwoByteString(const uint16_t* chars, int length);

 private:
  ConsStringIterator iter_;
  bool is_one_byte_;
  union {
    const uint8_t* buffer8_;
    const uint16_t* buffer16_;
  };
  const uint8_t* end_;
  SharedStringAccessGuardIfNeeded access_guard_;
};

uint16_t StringCharacterStream::GetNext() {
  DCHECK(buffer8_ != nullptr && end_ != nullptr);
  // Advance cursor if needed.
  if (buffer8_ == end_) HasMore();
  DCHECK(buffer8_ < end_);
  return is_one_byte_ ? *buffer8_++ : *buffer16_++;
}

// TODO(solanes, v8:7790, chromium:1166095): Assess if we need to use
// Isolate/LocalIsolate and pipe them through, instead of using the slow
// version of the SharedStringAccessGuardIfNeeded.
StringCharacterStream::StringCharacterStream(Tagged<String> string, int offset)
    : is_one_byte_(false), access_guard_(string) {
  Reset(string, offset);
}

void StringCharacterStream::Reset(Tagged<String> string, int offset) {
  buffer8_ = nullptr;
  end_ = nullptr;

  Tagged<ConsString> cons_string =
      String::VisitFlat(this, string, offset, access_guard_);
  iter_.Reset(cons_string, offset);
  if (!cons_string.is_null()) {
    string = iter_.Next(&offset);
    if (!string.is_null())
      String::VisitFlat(this, string, offset, access_guard_);
  }
}

bool StringCharacterStream::HasMore() {
  if (buffer8_ != end_) return true;
  int offset;
  Tagged<String> string = iter_.Next(&offset);
  DCHECK_EQ(offset, 0);
  if (string.is_null()) return false;
  String::VisitFlat(this, string, 0, access_guard_);
  DCHECK(buffer8_ != end_);
  return true;
}

void StringCharacterStream::VisitOneByteString(const uint8_t* chars,
                                               int length) {
  is_one_byte_ = true;
  buffer8_ = chars;
  end_ = chars + length;
}

void StringCharacterStream::VisitTwoByteString(const uint16_t* chars,
                                               int length) {
  is_one_byte_ = false;
  buffer16_ = chars;
  end_ = reinterpret_cast<const uint8_t*>(chars + length);
}

bool String::AsArrayIndex(uint32_t* index) {
  DisallowGarbageCollection no_gc;
  uint32_t field = raw_hash_field();
  if (ContainsCachedArrayIndex(field)) {
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (IsHashFieldComputed(field) && !IsIntegerIndex(field)) {
    return false;
  }
  return SlowAsArrayIndex(index);
}

bool String::AsIntegerIndex(size_t* index) {
  uint32_t field = raw_hash_field();
  if (ContainsCachedArrayIndex(field)) {
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (IsHashFieldComputed(field) && !IsIntegerIndex(field)) {
    return false;
  }
  return SlowAsIntegerIndex(index);
}

SubStringRange::SubStringRange(Tagged<String> string,
                               const DisallowGarbageCollection& no_gc,
                               int first, int length)
    : string_(string),
      first_(first),
      length_(length == -1 ? string->length() : length),
      no_gc_(no_gc) {}

class SubStringRange::iterator final {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = int;
  using value_type = base::uc16;
  using pointer = base::uc16*;
  using reference = base::uc16&;

  iterator(const iterator& other) = default;

  base::uc16 operator*() { return content_.Get(offset_); }
  bool operator==(const iterator& other) const {
    return content_.UsesSameString(other.content_) && offset_ == other.offset_;
  }
  bool operator!=(const iterator& other) const {
    return !content_.UsesSameString(other.content_) || offset_ != other.offset_;
  }
  iterator& operator++() {
    ++offset_;
    return *this;
  }
  iterator operator++(int);

 private:
  friend class String;
  friend class SubStringRange;
  iterator(Tagged<String> from, int offset,
           const DisallowGarbageCollection& no_gc)
      : content_(from->GetFlatContent(no_gc)), offset_(offset) {}
  String::FlatContent content_;
  int offset_;
};

SubStringRange::iterator SubStringRange::begin() {
  return SubStringRange::iterator(string_, first_, no_gc_);
}

SubStringRange::iterator SubStringRange::end() {
  return SubStringRange::iterator(string_, first_ + length_, no_gc_);
}

void SeqOneByteString::clear_padding_destructively(int length) {
  // Ensure we are not killing the map word, which is already set at this point
  static_assert(SizeFor(0) >= kObjectAlignment + kTaggedSize);
  memset(
      reinterpret_cast<void*>(address() + SizeFor(length) - kObjectAlignment),
      0, kObjectAlignment);
}

void SeqTwoByteString::clear_padding_destructively(int length) {
  // Ensure we are not killing the map word, which is already set at this point
  static_assert(SizeFor(0) >= kObjectAlignment + kTaggedSize);
  memset(
      reinterpret_cast<void*>(address() + SizeFor(length) - kObjectAlignment),
      0, kObjectAlignment);
}

// static
bool String::IsInPlaceInternalizable(Tagged<String> string) {
  return IsInPlaceInternalizable(string->map()->instance_type());
}

// static
bool String::IsInPlaceInternalizable(InstanceType instance_type) {
  switch (instance_type) {
    case SEQ_TWO_BYTE_STRING_TYPE:
    case SEQ_ONE_BYTE_STRING_TYPE:
    case SHARED_SEQ_TWO_BYTE_STRING_TYPE:
    case SHARED_SEQ_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_TWO_BYTE_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SHARED_EXTERNAL_TWO_BYTE_STRING_TYPE:
    case SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      return true;
    default:
      return false;
  }
}

// static
bool String::IsInPlaceInternalizableExcludingExternal(
    InstanceType instance_type) {
  return IsInPlaceInternalizable(instance_type) &&
         !InstanceTypeChecker::IsExternalString(instance_type);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_INL_H_
