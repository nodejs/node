// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NAME_H_
#define V8_OBJECTS_NAME_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// The Name abstract class captures anything that can be used as a property
// name, i.e., strings and symbols.  All names store a hash value.
class Name : public HeapObject {
 public:
  // Get and set the hash field of the name.
  inline uint32_t hash_field();
  inline void set_hash_field(uint32_t value);

  // Tells whether the hash code has been computed.
  inline bool HasHashCode();

  // Returns a hash value used for the property table
  inline uint32_t Hash();

  // Equality operations.
  inline bool Equals(Name* other);
  inline static bool Equals(Isolate* isolate, Handle<Name> one,
                            Handle<Name> two);

  // Conversion.
  inline bool AsArrayIndex(uint32_t* index);

  // An "interesting symbol" is a well-known symbol, like @@toStringTag,
  // that's often looked up on random objects but is usually not present.
  // We optimize this by setting a flag on the object's map when such
  // symbol properties are added, so we can optimize lookups on objects
  // that don't have the flag.
  inline bool IsInterestingSymbol() const;

  // If the name is private, it can only name own properties.
  inline bool IsPrivate();

  // If the name is a private field, it should behave like a private
  // symbol but also throw on property access miss.
  inline bool IsPrivateField();

  inline bool IsUniqueName() const;

  static inline bool ContainsCachedArrayIndex(uint32_t hash);

  // Return a string version of this name that is converted according to the
  // rules described in ES6 section 9.2.11.
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToFunctionName(
      Isolate* isolate, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> ToFunctionName(
      Isolate* isolate, Handle<Name> name, Handle<String> prefix);

  DECL_CAST(Name)

  DECL_PRINTER(Name)
  void NameShortPrint();
  int NameShortPrint(Vector<char> str);

  // Layout description.
  static const int kHashFieldOffset = HeapObject::kHeaderSize;
  static const int kHeaderSize = kHashFieldOffset + kInt32Size;

  // Mask constant for checking if a name has a computed hash code
  // and if it is a string that is an array index.  The least significant bit
  // indicates whether a hash code has been computed.  If the hash code has
  // been computed the 2nd bit tells whether the string can be used as an
  // array index.
  static const int kHashNotComputedMask = 1;
  static const int kIsNotArrayIndexMask = 1 << 1;
  static const int kNofHashBitFields = 2;

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

  // For strings which are array indexes the hash value has the string length
  // mixed into the hash, mainly to avoid a hash value of zero which would be
  // the case for the string '0'. 24 bits are used for the array index value.
  static const int kArrayIndexValueBits = 24;
  static const int kArrayIndexLengthBits =
      kBitsPerInt - kArrayIndexValueBits - kNofHashBitFields;

  STATIC_ASSERT(kArrayIndexLengthBits > 0);
  STATIC_ASSERT(kMaxArrayIndexSize < (1 << kArrayIndexLengthBits));

  class ArrayIndexValueBits
      : public BitField<unsigned int, kNofHashBitFields, kArrayIndexValueBits> {
  };  // NOLINT
  class ArrayIndexLengthBits
      : public BitField<unsigned int, kNofHashBitFields + kArrayIndexValueBits,
                        kArrayIndexLengthBits> {};  // NOLINT

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
      kIsNotArrayIndexMask | kHashNotComputedMask;

 protected:
  static inline bool IsHashFieldComputed(uint32_t field);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Name);
};

// ES6 symbols.
class Symbol : public Name {
 public:
  // [name]: The print name of a symbol, or undefined if none.
  DECL_ACCESSORS(name, Object)

  DECL_INT_ACCESSORS(flags)

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

  // [is_public]: Whether this is a symbol created by Symbol.for. Calling
  // Symbol.keyFor on such a symbol simply needs to return the attached name.
  DECL_BOOLEAN_ACCESSORS(is_public)

  // [is_private_field]: Whether this is a private field.  Private fields
  // are the same as private symbols except they throw on missing
  // property access.
  //
  // This also sets the is_private bit.
  inline bool is_private_field() const;
  inline void set_is_private_field();

  DECL_CAST(Symbol)

  // Dispatched behavior.
  DECL_PRINTER(Symbol)
  DECL_VERIFIER(Symbol)

  // Layout description.
  static const int kFlagsOffset = Name::kHeaderSize;
  static const int kNameOffset = kFlagsOffset + kInt32Size;
  static const int kSize = kNameOffset + kPointerSize;

// Flags layout.
#define FLAGS_BIT_FIELDS(V, _)          \
  V(IsPrivateBit, bool, 1, _)           \
  V(IsWellKnownSymbolBit, bool, 1, _)   \
  V(IsPublicBit, bool, 1, _)            \
  V(IsInterestingSymbolBit, bool, 1, _) \
  V(IsPrivateFieldBit, bool, 1, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  typedef FixedBodyDescriptor<kNameOffset, kSize, kSize> BodyDescriptor;

  void SymbolShortPrint(std::ostream& os);

 private:
  const char* PrivateSymbolToName() const;

  // TODO(cbruni): remove once the new maptracer is in place.
  friend class Name;  // For PrivateSymbolToName.

  DISALLOW_IMPLICIT_CONSTRUCTORS(Symbol);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_NAME_H_
