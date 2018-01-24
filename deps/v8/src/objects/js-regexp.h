// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_H_
#define V8_OBJECTS_JS_REGEXP_H_

#include "src/objects/js-array.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Regular expressions
// The regular expression holds a single reference to a FixedArray in
// the kDataOffset field.
// The FixedArray contains the following data:
// - tag : type of regexp implementation (not compiled yet, atom or irregexp)
// - reference to the original source string
// - reference to the original flag string
// If it is an atom regexp
// - a reference to a literal string to search for
// If it is an irregexp regexp:
// - a reference to code for Latin1 inputs (bytecode or compiled), or a smi
// used for tracking the last usage (used for regexp code flushing).
// - a reference to code for UC16 inputs (bytecode or compiled), or a smi
// used for tracking the last usage (used for regexp code flushing).
// - max number of registers used by irregexp implementations.
// - number of capture registers (output values) of the regexp.
class JSRegExp : public JSObject {
 public:
  // Meaning of Type:
  // NOT_COMPILED: Initial value. No data has been stored in the JSRegExp yet.
  // ATOM: A simple string to match against using an indexOf operation.
  // IRREGEXP: Compiled with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };
  enum Flag {
    kNone = 0,
    kGlobal = 1 << 0,
    kIgnoreCase = 1 << 1,
    kMultiline = 1 << 2,
    kSticky = 1 << 3,
    kUnicode = 1 << 4,
    kDotAll = 1 << 5,
    // Update FlagCount when adding new flags.
  };
  typedef base::Flags<Flag> Flags;

  static int FlagCount() { return 6; }

  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(flags, Object)
  DECL_ACCESSORS(last_index, Object)
  DECL_ACCESSORS(source, Object)

  V8_EXPORT_PRIVATE static MaybeHandle<JSRegExp> New(Handle<String> source,
                                                     Flags flags);
  static Handle<JSRegExp> Copy(Handle<JSRegExp> regexp);

  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source, Flags flags);
  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source,
                                          Handle<String> flags_string);

  inline Type TypeTag();
  // Number of captures (without the match itself).
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String* Pattern();
  inline Object* CaptureNameMap();
  inline Object* DataAt(int index);
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object* value);

  static int code_index(bool is_latin1) {
    if (is_latin1) {
      return kIrregexpLatin1CodeIndex;
    } else {
      return kIrregexpUC16CodeIndex;
    }
  }

  DECL_CAST(JSRegExp)

  // Dispatched behavior.
  DECL_PRINTER(JSRegExp)
  DECL_VERIFIER(JSRegExp)

  static const int kDataOffset = JSObject::kHeaderSize;
  static const int kSourceOffset = kDataOffset + kPointerSize;
  static const int kFlagsOffset = kSourceOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;
  static const int kLastIndexOffset = kSize;  // In-object field.

  // Indices in the data array.
  static const int kTagIndex = 0;
  static const int kSourceIndex = kTagIndex + 1;
  static const int kFlagsIndex = kSourceIndex + 1;
  static const int kDataIndex = kFlagsIndex + 1;
  // The data fields are used in different ways depending on the
  // value of the tag.
  // Atom regexps (literal strings).
  static const int kAtomPatternIndex = kDataIndex;

  static const int kAtomDataSize = kAtomPatternIndex + 1;

  // Irregexp compiled code or bytecode for Latin1. If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpLatin1CodeIndex = kDataIndex;
  // Irregexp compiled code or bytecode for UC16.  If compilation
  // fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpUC16CodeIndex = kDataIndex + 1;
  // Maximal number of registers used by either Latin1 or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 2;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 3;
  // Maps names of named capture groups (at indices 2i) to their corresponding
  // (1-based) capture group indices (at indices 2i + 1).
  static const int kIrregexpCaptureNameMapIndex = kDataIndex + 4;

  static const int kIrregexpDataSize = kIrregexpCaptureNameMapIndex + 1;

  // In-object fields.
  static const int kLastIndexFieldIndex = 0;
  static const int kInObjectFieldCount = 1;

  // The uninitialized value for a regexp code object.
  static const int kUninitializedValue = -1;
};

DEFINE_OPERATORS_FOR_FLAGS(JSRegExp::Flags)

// JSRegExpResult is just a JSArray with a specific initial map.
// This initial map adds in-object properties for "index" and "input"
// properties, as assigned by RegExp.prototype.exec, which allows
// faster creation of RegExp exec results.
// This class just holds constants used when creating the result.
// After creation the result must be treated as a JSArray in all regards.
class JSRegExpResult : public JSArray {
 public:
  // Offsets of object fields.
  static const int kIndexOffset = JSArray::kSize;
  static const int kInputOffset = kIndexOffset + kPointerSize;
  static const int kSize = kInputOffset + kPointerSize;
  // Indices of in-object properties.
  static const int kIndexIndex = 0;
  static const int kInputIndex = 1;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSRegExpResult);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_H_
