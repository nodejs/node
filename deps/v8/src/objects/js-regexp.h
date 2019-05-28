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
  struct FlagShiftBit {
    static const int kGlobal = 0;
    static const int kIgnoreCase = 1;
    static const int kMultiline = 2;
    static const int kSticky = 3;
    static const int kUnicode = 4;
    static const int kDotAll = 5;
    static const int kInvalid = 7;
  };
  enum Flag : uint8_t {
    kNone = 0,
    kGlobal = 1 << FlagShiftBit::kGlobal,
    kIgnoreCase = 1 << FlagShiftBit::kIgnoreCase,
    kMultiline = 1 << FlagShiftBit::kMultiline,
    kSticky = 1 << FlagShiftBit::kSticky,
    kUnicode = 1 << FlagShiftBit::kUnicode,
    kDotAll = 1 << FlagShiftBit::kDotAll,
    // Update FlagCount when adding new flags.
    kInvalid = 1 << FlagShiftBit::kInvalid,  // Not included in FlagCount.
  };
  using Flags = base::Flags<Flag>;
  static constexpr int FlagCount() { return 6; }

  static int FlagShiftBits(Flag flag) {
    switch (flag) {
      case kGlobal:
        return FlagShiftBit::kGlobal;
      case kIgnoreCase:
        return FlagShiftBit::kIgnoreCase;
      case kMultiline:
        return FlagShiftBit::kMultiline;
      case kSticky:
        return FlagShiftBit::kSticky;
      case kUnicode:
        return FlagShiftBit::kUnicode;
      case kDotAll:
        return FlagShiftBit::kDotAll;
      default:
        STATIC_ASSERT(FlagCount() == 6);
        UNREACHABLE();
    }
  }

  DECL_ACCESSORS(data, Object)
  DECL_ACCESSORS(flags, Object)
  DECL_ACCESSORS(last_index, Object)
  DECL_ACCESSORS(source, Object)

  V8_EXPORT_PRIVATE static MaybeHandle<JSRegExp> New(Isolate* isolate,
                                                     Handle<String> source,
                                                     Flags flags);
  static Handle<JSRegExp> Copy(Handle<JSRegExp> regexp);

  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source, Flags flags);
  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source,
                                          Handle<String> flags_string);

  inline Type TypeTag() const;
  // Number of captures (without the match itself).
  inline int CaptureCount();
  inline Flags GetFlags();
  inline String Pattern();
  inline Object CaptureNameMap();
  inline Object DataAt(int index) const;
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object value);

  static int code_index(bool is_latin1) {
    if (is_latin1) {
      return kIrregexpLatin1CodeIndex;
    } else {
      return kIrregexpUC16CodeIndex;
    }
  }

  inline bool HasCompiledCode() const;
  inline void DiscardCompiledCodeForSerialization();

  DECL_CAST(JSRegExp)

  // Dispatched behavior.
  DECL_PRINTER(JSRegExp)
  DECL_VERIFIER(JSRegExp)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSREG_EXP_FIELDS)
  /* This is already an in-object field. */
  // TODO(v8:8944): improve handling of in-object fields
  static constexpr int kLastIndexOffset = kSize;

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

  // Descriptor array index to important methods in the prototype.
  static const int kExecFunctionDescriptorIndex = 1;
  static const int kSymbolMatchFunctionDescriptorIndex = 13;
  static const int kSymbolReplaceFunctionDescriptorIndex = 14;
  static const int kSymbolSearchFunctionDescriptorIndex = 15;
  static const int kSymbolSplitFunctionDescriptorIndex = 16;
  static const int kSymbolMatchAllFunctionDescriptorIndex = 17;

  // The uninitialized value for a regexp code object.
  static const int kUninitializedValue = -1;

  OBJECT_CONSTRUCTORS(JSRegExp, JSObject);
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
  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSArray::kSize,
                                TORQUE_GENERATED_JSREG_EXP_RESULT_FIELDS)

  // Indices of in-object properties.
  static const int kIndexIndex = 0;
  static const int kInputIndex = 1;
  static const int kGroupsIndex = 2;
  static const int kInObjectPropertyCount = 3;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSRegExpResult);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_H_
