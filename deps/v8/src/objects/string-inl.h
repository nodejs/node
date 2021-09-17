// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_INL_H_
#define V8_OBJECTS_STRING_INL_H_

#include "src/common/assert-scope.h"
#include "src/common/external-pointer-inl.h"
#include "src/common/external-pointer.h"
#include "src/common/globals.h"
#include "src/execution/isolate-utils.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-table-inl.h"
#include "src/objects/string.h"
#include "src/strings/string-hasher-inl.h"
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
  explicit SharedStringAccessGuardIfNeeded(String str) {
    Isolate* isolate = GetIsolateIfNeeded(str);
    if (isolate != nullptr)
      mutex_guard.emplace(isolate->internalized_string_access());
  }

  static SharedStringAccessGuardIfNeeded NotNeeded() {
    return SharedStringAccessGuardIfNeeded();
  }

#ifdef DEBUG
  static bool IsNeeded(String str) {
    return GetIsolateIfNeeded(str) != nullptr;
  }
#endif

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
  static Isolate* GetIsolateIfNeeded(String str) {
    LocalHeap* local_heap = LocalHeap::Current();
    // Don't acquire the lock for the main thread.
    if (!local_heap || local_heap->is_main_thread()) return nullptr;

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

StringShape::StringShape(const String str)
    : type_(str.map(kAcquireLoad).instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(Map map) : type_(map.instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(InstanceType t) : type_(static_cast<uint32_t>(t)) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

bool StringShape::IsInternalized() const {
  DCHECK(valid());
  STATIC_ASSERT(kNotInternalizedTag != 0);
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

bool StringShape::IsExternal() const {
  return (type_ & kStringRepresentationMask) == kExternalStringTag;
}

bool StringShape::IsSequential() const {
  return (type_ & kStringRepresentationMask) == kSeqStringTag;
}

bool StringShape::IsUncachedExternal() const {
  return (type_ & kUncachedExternalStringMask) == kUncachedExternalStringTag;
}

StringRepresentationTag StringShape::representation_tag() const {
  uint32_t tag = (type_ & kStringRepresentationMask);
  return static_cast<StringRepresentationTag>(tag);
}

uint32_t StringShape::encoding_tag() const {
  return type_ & kStringEncodingMask;
}

uint32_t StringShape::full_representation_tag() const {
  return (type_ & (kStringRepresentationMask | kStringEncodingMask));
}

STATIC_ASSERT((kStringRepresentationMask | kStringEncodingMask) ==
              Internals::kFullStringRepresentationMask);

STATIC_ASSERT(static_cast<uint32_t>(kStringEncodingMask) ==
              Internals::kStringEncodingMask);

bool StringShape::IsSequentialOneByte() const {
  return full_representation_tag() == (kSeqStringTag | kOneByteStringTag);
}

bool StringShape::IsSequentialTwoByte() const {
  return full_representation_tag() == (kSeqStringTag | kTwoByteStringTag);
}

bool StringShape::IsExternalOneByte() const {
  return full_representation_tag() == (kExternalStringTag | kOneByteStringTag);
}

STATIC_ASSERT((kExternalStringTag | kOneByteStringTag) ==
              Internals::kExternalOneByteRepresentationTag);

STATIC_ASSERT(v8::String::ONE_BYTE_ENCODING == kOneByteStringTag);

bool StringShape::IsExternalTwoByte() const {
  return full_representation_tag() == (kExternalStringTag | kTwoByteStringTag);
}

STATIC_ASSERT((kExternalStringTag | kTwoByteStringTag) ==
              Internals::kExternalTwoByteRepresentationTag);

STATIC_ASSERT(v8::String::TWO_BYTE_ENCODING == kTwoByteStringTag);

template <typename TDispatcher, typename TResult, typename... TArgs>
inline TResult StringShape::DispatchToSpecificTypeWithoutCast(TArgs&&... args) {
  switch (full_representation_tag()) {
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
inline TResult StringShape::DispatchToSpecificType(String str,
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
    static inline TResult HandleInvalidString(String str, TArgs&&... args) {
      return TDispatcher::HandleInvalidString(str,
                                              std::forward<TArgs>(args)...);
    }
  };

  return DispatchToSpecificTypeWithoutCast<CastingDispatcher, TResult>(
      str, std::forward<TArgs>(args)...);
}

DEF_GETTER(String, IsOneByteRepresentation, bool) {
  uint32_t type = map(cage_base).instance_type();
  return (type & kStringEncodingMask) == kOneByteStringTag;
}

DEF_GETTER(String, IsTwoByteRepresentation, bool) {
  uint32_t type = map(cage_base).instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}

// static
bool String::IsOneByteRepresentationUnderneath(String string) {
  while (true) {
    uint32_t type = string.map().instance_type();
    STATIC_ASSERT(kIsIndirectStringTag != 0);
    STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
    DCHECK(string.IsFlat());
    switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
      case kOneByteStringTag:
        return true;
      case kTwoByteStringTag:
        return false;
      default:  // Cons, sliced, thin, strings need to go deeper.
        string = string.GetUnderlying();
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
  SequentialStringKey(const base::Vector<const Char>& chars, uint64_t seed,
                      bool convert = false)
      : SequentialStringKey(StringHasher::HashSequentialString<Char>(
                                chars.begin(), chars.length(), seed),
                            chars, convert) {}

  SequentialStringKey(int raw_hash_field, const base::Vector<const Char>& chars,
                      bool convert = false)
      : StringTableKey(raw_hash_field, chars.length()),
        chars_(chars),
        convert_(convert) {}

  template <typename IsolateT>
  bool IsMatch(IsolateT* isolate, String s) {
    return s.IsEqualTo<String::EqualityType::kNoLengthCheck>(chars_, isolate);
  }

  Handle<String> AsHandle(Isolate* isolate) {
    if (sizeof(Char) == 1) {
      return isolate->factory()->NewOneByteInternalizedString(
          base::Vector<const uint8_t>::cast(chars_), raw_hash_field());
    }
    return isolate->factory()->NewTwoByteInternalizedString(
        base::Vector<const uint16_t>::cast(chars_), raw_hash_field());
  }

  Handle<String> AsHandle(LocalIsolate* isolate) {
    if (sizeof(Char) == 1) {
      return isolate->factory()->NewOneByteInternalizedString(
          base::Vector<const uint8_t>::cast(chars_), raw_hash_field());
    }
    return isolate->factory()->NewTwoByteInternalizedString(
        base::Vector<const uint16_t>::cast(chars_), raw_hash_field());
  }

 private:
  base::Vector<const Char> chars_;
  bool convert_;
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
    DCHECK_EQ(string_->IsSeqOneByteString(), sizeof(Char) == 1);
    DCHECK_EQ(string_->IsSeqTwoByteString(), sizeof(Char) == 2);
  }
#if defined(V8_CC_MSVC)
#pragma warning(pop)
#endif

  bool IsMatch(Isolate* isolate, String string) {
    DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(string));
    DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*string_));
    DisallowGarbageCollection no_gc;
    return string.IsEqualTo<String::EqualityType::kNoLengthCheck>(
        base::Vector<const Char>(string_->GetChars(no_gc) + from_, length()),
        isolate);
  }

  Handle<String> AsHandle(Isolate* isolate) {
    if (sizeof(Char) == 1 || (sizeof(Char) == 2 && convert_)) {
      Handle<SeqOneByteString> result =
          isolate->factory()->AllocateRawOneByteInternalizedString(
              length(), raw_hash_field());
      DisallowGarbageCollection no_gc;
      CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
                length());
      return result;
    }
    Handle<SeqTwoByteString> result =
        isolate->factory()->AllocateRawTwoByteInternalizedString(
            length(), raw_hash_field());
    DisallowGarbageCollection no_gc;
    CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
              length());
    return result;
  }

 private:
  Handle<typename CharTraits<Char>::String> string_;
  int from_;
  bool convert_;
};

using SeqOneByteSubStringKey = SeqSubStringKey<SeqOneByteString>;
using SeqTwoByteSubStringKey = SeqSubStringKey<SeqTwoByteString>;

bool String::Equals(String other) const {
  if (other == *this) return true;
  if (this->IsInternalizedString() && other.IsInternalizedString()) {
    return false;
  }
  return SlowEquals(other);
}

// static
bool String::Equals(Isolate* isolate, Handle<String> one, Handle<String> two) {
  if (one.is_identical_to(two)) return true;
  if (one->IsInternalizedString() && two->IsInternalizedString()) {
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
  String string = *this;
  const Char* data = str.data();
  while (true) {
    int32_t type = string.map(cage_base).instance_type();
    switch (type & (kStringRepresentationMask | kStringEncodingMask)) {
      case kSeqStringTag | kOneByteStringTag:
        return CompareCharsEqual(
            SeqOneByteString::cast(string).GetChars(no_gc, access_guard) +
                slice_offset,
            data, len);
      case kSeqStringTag | kTwoByteStringTag:
        return CompareCharsEqual(
            SeqTwoByteString::cast(string).GetChars(no_gc, access_guard) +
                slice_offset,
            data, len);
      case kExternalStringTag | kOneByteStringTag:
        return CompareCharsEqual(
            ExternalOneByteString::cast(string).GetChars() + slice_offset, data,
            len);
      case kExternalStringTag | kTwoByteStringTag:
        return CompareCharsEqual(
            ExternalTwoByteString::cast(string).GetChars() + slice_offset, data,
            len);

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        SlicedString slicedString = SlicedString::cast(string);
        slice_offset += slicedString.offset();
        string = slicedString.parent(cage_base);
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag: {
        // The ConsString path is more complex and rare, so call out to an
        // out-of-line handler.
        return IsConsStringEqualToImpl<Char>(ConsString::cast(string),
                                             slice_offset, str, cage_base,
                                             access_guard);
      }

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string).actual(cage_base);
        continue;

      default:
        UNREACHABLE();
    }
  }
}

// static
template <typename Char>
bool String::IsConsStringEqualToImpl(
    ConsString string, int slice_offset, base::Vector<const Char> str,
    PtrComprCageBase cage_base,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  // Already checked the len in IsEqualToImpl. Check GE rather than EQ in case
  // this is a prefix check.
  DCHECK_GE(string.length(), str.size());

  ConsStringIterator iter(ConsString::cast(string), slice_offset);
  base::Vector<const Char> remaining_str = str;
  for (String segment = iter.Next(&slice_offset); !segment.is_null();
       segment = iter.Next(&slice_offset)) {
    // Compare the individual segment against the appropriate subvector of the
    // remaining string.
    size_t len = std::min<size_t>(segment.length(), remaining_str.size());
    base::Vector<const Char> sub_str = remaining_str.SubVector(0, len);
    if (!segment.IsEqualToImpl<EqualityType::kNoLengthCheck>(sub_str, cage_base,
                                                             access_guard)) {
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
const Char* String::GetChars(const DisallowGarbageCollection& no_gc) const {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(*this));
  return StringShape(*this).IsExternal()
             ? CharTraits<Char>::ExternalString::cast(*this).GetChars()
             : CharTraits<Char>::String::cast(*this).GetChars(no_gc);
}

template <typename Char>
const Char* String::GetChars(
    const DisallowGarbageCollection& no_gc,
    const SharedStringAccessGuardIfNeeded& access_guard) const {
  return StringShape(*this).IsExternal()
             ? CharTraits<Char>::ExternalString::cast(*this).GetChars()
             : CharTraits<Char>::String::cast(*this).GetChars(no_gc,
                                                              access_guard);
}

Handle<String> String::Flatten(Isolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  if (string->IsConsString()) {
    Handle<ConsString> cons = Handle<ConsString>::cast(string);
    if (cons->IsFlat()) {
      string = handle(cons->first(), isolate);
    } else {
      return SlowFlatten(isolate, cons, allocation);
    }
  }
  if (string->IsThinString()) {
    string = handle(Handle<ThinString>::cast(string)->actual(), isolate);
    DCHECK(!string->IsConsString());
  }
  return string;
}

Handle<String> String::Flatten(LocalIsolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  // We should never pass non-flat strings to String::Flatten when off-thread.
  DCHECK(string->IsFlat());
  return string;
}

uint16_t String::Get(int index, Isolate* isolate) const {
  SharedStringAccessGuardIfNeeded scope(isolate);
  return GetImpl(index, scope);
}

uint16_t String::Get(int index, LocalIsolate* local_isolate) const {
  SharedStringAccessGuardIfNeeded scope(local_isolate);
  return GetImpl(index, scope);
}

uint16_t String::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  return GetImpl(index, access_guard);
}

uint16_t String::GetImpl(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  DCHECK(index >= 0 && index < length());

  class StringGetDispatcher : public AllStatic {
   public:
#define DEFINE_METHOD(Type)                                  \
  static inline uint16_t Handle##Type(                       \
      Type str, int index,                                   \
      const SharedStringAccessGuardIfNeeded& access_guard) { \
    return str.Get(index, access_guard);                     \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline uint16_t HandleInvalidString(
        String str, int index,
        const SharedStringAccessGuardIfNeeded& access_guard) {
      UNREACHABLE();
    }
  };

  return StringShape(*this)
      .DispatchToSpecificType<StringGetDispatcher, uint16_t>(*this, index,
                                                             access_guard);
}

void String::Set(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  DCHECK(StringShape(*this).IsSequential());

  return this->IsOneByteRepresentation()
             ? SeqOneByteString::cast(*this).SeqOneByteStringSet(index, value)
             : SeqTwoByteString::cast(*this).SeqTwoByteStringSet(index, value);
}

bool String::IsFlat() const {
  if (!StringShape(*this).IsCons()) return true;
  return ConsString::cast(*this).second().length() == 0;
}

String String::GetUnderlying() const {
  // Giving direct access to underlying string only makes sense if the
  // wrapping string is already flattened.
  DCHECK(this->IsFlat());
  DCHECK(StringShape(*this).IsIndirect());
  STATIC_ASSERT(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(SlicedString::kParentOffset));
  STATIC_ASSERT(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(ThinString::kActualOffset));
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return TaggedField<String, kUnderlyingOffset>::load(*this);
}

template <class Visitor>
ConsString String::VisitFlat(Visitor* visitor, String string,
                             const int offset) {
  DCHECK(!SharedStringAccessGuardIfNeeded::IsNeeded(string));
  return VisitFlat(visitor, string, offset,
                   SharedStringAccessGuardIfNeeded::NotNeeded());
}

template <class Visitor>
ConsString String::VisitFlat(
    Visitor* visitor, String string, const int offset,
    const SharedStringAccessGuardIfNeeded& access_guard) {
  DisallowGarbageCollection no_gc;
  int slice_offset = offset;
  const int length = string.length();
  DCHECK(offset <= length);
  while (true) {
    int32_t type = string.map().instance_type();
    switch (type & (kStringRepresentationMask | kStringEncodingMask)) {
      case kSeqStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            SeqOneByteString::cast(string).GetChars(no_gc, access_guard) +
                slice_offset,
            length - offset);
        return ConsString();

      case kSeqStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            SeqTwoByteString::cast(string).GetChars(no_gc, access_guard) +
                slice_offset,
            length - offset);
        return ConsString();

      case kExternalStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            ExternalOneByteString::cast(string).GetChars() + slice_offset,
            length - offset);
        return ConsString();

      case kExternalStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            ExternalTwoByteString::cast(string).GetChars() + slice_offset,
            length - offset);
        return ConsString();

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        SlicedString slicedString = SlicedString::cast(string);
        slice_offset += slicedString.offset();
        string = slicedString.parent();
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag:
        return ConsString::cast(string);

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string).actual();
        continue;

      default:
        UNREACHABLE();
    }
  }
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
  return Get(index, SharedStringAccessGuardIfNeeded::NotNeeded());
}

uint8_t SeqOneByteString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return ReadField<byte>(kHeaderSize + index * kCharSize);
}

void SeqOneByteString::SeqOneByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length() && value <= kMaxOneByteCharCode);
  WriteField<byte>(kHeaderSize + index * kCharSize, static_cast<byte>(value));
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
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
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

void SlicedString::set_parent(String parent, WriteBarrierMode mode) {
  DCHECK(parent.IsSeqString() || parent.IsExternalString());
  TorqueGeneratedSlicedString<SlicedString, Super>::set_parent(parent, mode);
}

Object ConsString::unchecked_first() {
  return TaggedField<Object, kFirstOffset>::load(*this);
}

Object ConsString::unchecked_second() {
  return RELAXED_READ_FIELD(*this, kSecondOffset);
}

DEF_GETTER(ThinString, unchecked_actual, HeapObject) {
  return TaggedField<HeapObject, kActualOffset>::load(cage_base, *this);
}

bool ExternalString::is_uncached() const {
  InstanceType type = map().instance_type();
  return (type & kUncachedExternalStringMask) == kUncachedExternalStringTag;
}

void ExternalString::AllocateExternalPointerEntries(Isolate* isolate) {
  InitExternalPointerField(kResourceOffset, isolate);
  if (is_uncached()) return;
  InitExternalPointerField(kResourceDataOffset, isolate);
}

DEF_GETTER(ExternalString, resource_as_address, Address) {
  Isolate* isolate = GetIsolateForHeapSandbox(*this);
  return ReadExternalPointerField(kResourceOffset, isolate,
                                  kExternalStringResourceTag);
}

void ExternalString::set_address_as_resource(Isolate* isolate, Address value) {
  WriteExternalPointerField(kResourceOffset, isolate, value,
                            kExternalStringResourceTag);
  if (IsExternalOneByteString()) {
    ExternalOneByteString::cast(*this).update_data_cache(isolate);
  } else {
    ExternalTwoByteString::cast(*this).update_data_cache(isolate);
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
  Address value = ReadExternalPointerField(kResourceOffset, isolate,
                                           kExternalStringResourceTag);
  v8::String::ExternalStringResourceBase* resource =
      reinterpret_cast<v8::String::ExternalStringResourceBase*>(value);

  // Dispose of the C++ object if it has not already been disposed.
  if (resource != nullptr) {
    resource->Dispose();
    WriteExternalPointerField(kResourceOffset, isolate, kNullAddress,
                              kExternalStringResourceTag);
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
    if (resource()->IsCacheable()) mutable_resource()->UpdateDataCache();
  } else {
    WriteExternalPointerField(kResourceDataOffset, isolate,
                              reinterpret_cast<Address>(resource()->data()),
                              kExternalStringResourceDataTag);
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
  WriteExternalPointerField(kResourceOffset, isolate,
                            reinterpret_cast<Address>(resource),
                            kExternalStringResourceTag);
  if (resource != nullptr) update_data_cache(isolate);
}

const uint8_t* ExternalOneByteString::GetChars() const {
  DisallowGarbageCollection no_gc;
  if (is_uncached()) {
    if (resource()->IsCacheable()) {
      // TODO(solanes): Teach TurboFan/CSA to not bailout to the runtime to
      // avoid this call.
      return reinterpret_cast<const uint8_t*>(resource()->cached_data());
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

  return reinterpret_cast<const uint8_t*>(resource()->data());
}

uint8_t ExternalOneByteString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
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
    if (resource()->IsCacheable()) mutable_resource()->UpdateDataCache();
  } else {
    WriteExternalPointerField(kResourceDataOffset, isolate,
                              reinterpret_cast<Address>(resource()->data()),
                              kExternalStringResourceDataTag);
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
  WriteExternalPointerField(kResourceOffset, isolate,
                            reinterpret_cast<Address>(resource),
                            kExternalStringResourceTag);
  if (resource != nullptr) update_data_cache(isolate);
}

const uint16_t* ExternalTwoByteString::GetChars() const {
  DisallowGarbageCollection no_gc;
  if (is_uncached()) {
    if (resource()->IsCacheable()) {
      // TODO(solanes): Teach TurboFan/CSA to not bailout to the runtime to
      // avoid this call.
      return resource()->cached_data();
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

  return resource()->data();
}

uint16_t ExternalTwoByteString::Get(
    int index, const SharedStringAccessGuardIfNeeded& access_guard) const {
  USE(access_guard);
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
}

const uint16_t* ExternalTwoByteString::ExternalTwoByteStringGetData(
    unsigned start) {
  return GetChars() + start;
}

int ConsStringIterator::OffsetForDepth(int depth) { return depth & kDepthMask; }

void ConsStringIterator::PushLeft(ConsString string) {
  frames_[depth_++ & kDepthMask] = string;
}

void ConsStringIterator::PushRight(ConsString string) {
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
  inline explicit StringCharacterStream(String string, int offset = 0);
  StringCharacterStream(const StringCharacterStream&) = delete;
  StringCharacterStream& operator=(const StringCharacterStream&) = delete;
  inline uint16_t GetNext();
  inline bool HasMore();
  inline void Reset(String string, int offset = 0);
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
StringCharacterStream::StringCharacterStream(String string, int offset)
    : is_one_byte_(false), access_guard_(string) {
  Reset(string, offset);
}

void StringCharacterStream::Reset(String string, int offset) {
  buffer8_ = nullptr;
  end_ = nullptr;

  ConsString cons_string =
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
  String string = iter_.Next(&offset);
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
  if (IsHashFieldComputed(field) && (field & kIsNotIntegerIndexMask)) {
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
  if (IsHashFieldComputed(field) && (field & kIsNotIntegerIndexMask)) {
    return false;
  }
  return SlowAsIntegerIndex(index);
}

SubStringRange::SubStringRange(String string,
                               const DisallowGarbageCollection& no_gc,
                               int first, int length)
    : string_(string),
      first_(first),
      length_(length == -1 ? string.length() : length),
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
  iterator(String from, int offset, const DisallowGarbageCollection& no_gc)
      : content_(from.GetFlatContent(no_gc)), offset_(offset) {}
  String::FlatContent content_;
  int offset_;
};

SubStringRange::iterator SubStringRange::begin() {
  return SubStringRange::iterator(string_, first_, no_gc_);
}

SubStringRange::iterator SubStringRange::end() {
  return SubStringRange::iterator(string_, first_ + length_, no_gc_);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_INL_H_
