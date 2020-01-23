// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NAME_H_
#define V8_OBJECTS_NAME_H_

#include "src/objects/objects.h"
#include "src/objects/primitive-heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// The Name abstract class captures anything that can be used as a property
// name, i.e., strings and symbols.  All names store a hash value.
class Name : public TorqueGeneratedName<Name, PrimitiveHeapObject> {
 public:
  // Tells whether the hash code has been computed.
  inline bool HasHashCode();

  // Returns a hash value used for the property table
  inline uint32_t Hash();

  // Equality operations.
  inline bool Equals(Name other);
  inline static bool Equals(Isolate* isolate, Handle<Name> one,
                            Handle<Name> two);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);
  inline bool AsIntegerIndex(size_t* index);

  // An "interesting symbol" is a well-known symbol, like @@toStringTag,
  // that's often looked up on random objects but is usually not present.
  // We optimize this by setting a flag on the object's map when such
  // symbol properties are added, so we can optimize lookups on objects
  // that don't have the flag.
  inline bool IsInterestingSymbol() const;
  inline bool IsInterestingSymbol(Isolate* isolate) const;

  // If the name is private, it can only name own properties.
  inline bool IsPrivate() const;
  inline bool IsPrivate(Isolate* isolate) const;

  // If the name is a private name, it should behave like a private
  // symbol but also throw on property access miss.
  inline bool IsPrivateName() const;
  inline bool IsPrivateName(Isolate* isolate) const;

  inline bool IsUniqueName() const;
  inline bool IsUniqueName(Isolate* isolate) const;

  static inline bool ContainsCachedArrayIndex(uint32_t hash);

  // Return a string version of this name that is converted according to the
  // rules described in ES6 section 9.2.11.
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToFunctionName(
      Isolate* isolate, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToFunctionName(
      Isolate* isolate, Handle<Name> name, Handle<String> prefix);

  DECL_PRINTER(Name)
  void NameShortPrint();
  int NameShortPrint(Vector<char> str);

  // Mask constant for checking if a name has a computed hash code
  // and if it is a string that is an array index.  The least significant bit
  // indicates whether a hash code has been computed.  If the hash code has
  // been computed the 2nd bit tells whether the string can be used as an
  // array index.
  static const int kHashNotComputedMask = 1;
  static const int kIsNotArrayIndexMask = 1 << 1;
  static const int kIsNotIntegerIndexMask = 1 << 2;
  static const int kNofHashBitFields = 3;

  // Shift constant retrieving hash code from hash field.
  static const int kHashShift = kNofHashBitFields;

  // Only these bits are relevant in the hash, since the top two are shifted
  // out.
  static const uint32_t kHashBitMask = 0xffffffffu >> kHashShift;

  // Array index strings this short can keep their index in the hash field.
  static const int kMaxCachedArrayIndexLength = 7;

  // Maximum number of characters to consider when trying to convert a string
  // value into an array index.
  static const int kMaxArrayIndexSize = 10;
  // Maximum number of characters that might be parsed into a size_t:
  // 10 characters per 32 bits of size_t width.
  // We choose this as large as possible (rather than MAX_SAFE_INTEGER range)
  // because TypedArray accesses will treat all string keys that are
  // canonical representations of numbers in the range [MAX_SAFE_INTEGER ..
  // size_t::max] as out-of-bounds accesses, and we can handle those in the
  // fast path if we tag them as such (see kIsNotIntegerIndexMask).
  static const int kMaxIntegerIndexSize = 10 * (sizeof(size_t) / 4);

  // For strings which are array indexes the hash value has the string length
  // mixed into the hash, mainly to avoid a hash value of zero which would be
  // the case for the string '0'. 24 bits are used for the array index value.
  static const int kArrayIndexValueBits = 24;
  static const int kArrayIndexLengthBits =
      kBitsPerInt - kArrayIndexValueBits - kNofHashBitFields;

  STATIC_ASSERT(kArrayIndexLengthBits > 0);
  STATIC_ASSERT(kMaxArrayIndexSize < (1 << kArrayIndexLengthBits));

  using ArrayIndexValueBits =
      BitField<unsigned int, kNofHashBitFields, kArrayIndexValueBits>;
  using ArrayIndexLengthBits =
      BitField<unsigned int, kNofHashBitFields + kArrayIndexValueBits,
               kArrayIndexLengthBits>;

  // Check that kMaxCachedArrayIndexLength + 1 is a power of two so we
  // could use a mask to test if the length of string is less than or equal to
  // kMaxCachedArrayIndexLength.
  static_assert(base::bits::IsPowerOfTwo(kMaxCachedArrayIndexLength + 1),
                "(kMaxCachedArrayIndexLength + 1) must be power of two");

  // When any of these bits is set then the hash field does not contain a cached
  // array index.
  static const unsigned int kDoesNotContainCachedArrayIndexMask =
      (~static_cast<unsigned>(kMaxCachedArrayIndexLength)
       << ArrayIndexLengthBits::kShift) |
      kIsNotArrayIndexMask;

  // Value of empty hash field indicating that the hash is not computed.
  static const int kEmptyHashField =
      kIsNotIntegerIndexMask | kIsNotArrayIndexMask | kHashNotComputedMask;

 protected:
  static inline bool IsHashFieldComputed(uint32_t field);

  TQ_OBJECT_CONSTRUCTORS(Name)
};

// ES6 symbols.
class Symbol : public TorqueGeneratedSymbol<Symbol, Name> {
 public:
  // [is_private]: Whether this is a private symbol.  Private symbols can only
  // be used to designate own properties of objects.
  DECL_BOOLEAN_ACCESSORS(is_private)

  // [is_well_known_symbol]: Whether this is a spec-defined well-known symbol,
  // or not. Well-known symbols do not throw when an access check fails during
  // a load.
  DECL_BOOLEAN_ACCESSORS(is_well_known_symbol)

  // [is_interesting_symbol]: Whether this is an "interesting symbol", which
  // is a well-known symbol like @@toStringTag that's often looked up on
  // random objects but is usually not present. See Name::IsInterestingSymbol()
  // for a detailed description.
  DECL_BOOLEAN_ACCESSORS(is_interesting_symbol)

  // [is_in_public_symbol_table]: Whether this is a symbol created by
  // Symbol.for. Calling Symbol.keyFor on such a symbol simply needs
  // to return the attached name.
  DECL_BOOLEAN_ACCESSORS(is_in_public_symbol_table)

  // [is_private_name]: Whether this is a private name.  Private names
  // are the same as private symbols except they throw on missing
  // property access.
  //
  // This also sets the is_private bit.
  inline bool is_private_name() const;
  inline void set_is_private_name();

  // Dispatched behavior.
  DECL_PRINTER(Symbol)
  DECL_VERIFIER(Symbol)

// Flags layout.
#define FLAGS_BIT_FIELDS(V, _)            \
  V(IsPrivateBit, bool, 1, _)             \
  V(IsWellKnownSymbolBit, bool, 1, _)     \
  V(IsInPublicSymbolTableBit, bool, 1, _) \
  V(IsInterestingSymbolBit, bool, 1, _)   \
  V(IsPrivateNameBit, bool, 1, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  using BodyDescriptor = FixedBodyDescriptor<kNameOffset, kSize, kSize>;

  void SymbolShortPrint(std::ostream& os);

 private:
  const char* PrivateSymbolToName() const;

  // TODO(cbruni): remove once the new maptracer is in place.
  friend class Name;  // For PrivateSymbolToName.

  TQ_OBJECT_CONSTRUCTORS(Symbol)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NAME_H_
