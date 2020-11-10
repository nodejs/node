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
class JSRegExp : public TorqueGeneratedJSRegExp<JSRegExp, JSObject> {
 public:
  // Meaning of Type:
  // NOT_COMPILED: Initial value. No data has been stored in the JSRegExp yet.
  // ATOM: A simple string to match against using an indexOf operation.
  // IRREGEXP: Compiled with Irregexp.
  enum Type { NOT_COMPILED, ATOM, IRREGEXP };
  struct FlagShiftBit {
    static constexpr int kGlobal = 0;
    static constexpr int kIgnoreCase = 1;
    static constexpr int kMultiline = 2;
    static constexpr int kSticky = 3;
    static constexpr int kUnicode = 4;
    static constexpr int kDotAll = 5;
    static constexpr int kInvalid = 6;
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

  static constexpr int kFlagCount = 6;

  static constexpr Flag FlagFromChar(char c) {
    STATIC_ASSERT(kFlagCount == 6);
    // clang-format off
    return c == 'g' ? kGlobal
         : c == 'i' ? kIgnoreCase
         : c == 'm' ? kMultiline
         : c == 'y' ? kSticky
         : c == 'u' ? kUnicode
         : c == 's' ? kDotAll
         : kInvalid;
    // clang-format on
  }

  STATIC_ASSERT(static_cast<int>(kNone) == v8::RegExp::kNone);
  STATIC_ASSERT(static_cast<int>(kGlobal) == v8::RegExp::kGlobal);
  STATIC_ASSERT(static_cast<int>(kIgnoreCase) == v8::RegExp::kIgnoreCase);
  STATIC_ASSERT(static_cast<int>(kMultiline) == v8::RegExp::kMultiline);
  STATIC_ASSERT(static_cast<int>(kSticky) == v8::RegExp::kSticky);
  STATIC_ASSERT(static_cast<int>(kUnicode) == v8::RegExp::kUnicode);
  STATIC_ASSERT(static_cast<int>(kDotAll) == v8::RegExp::kDotAll);
  STATIC_ASSERT(kFlagCount == v8::RegExp::kFlagCount);

  DECL_ACCESSORS(last_index, Object)

  // If the backtrack limit is set to this marker value, no limit is applied.
  static constexpr uint32_t kNoBacktrackLimit = 0;

  V8_EXPORT_PRIVATE static MaybeHandle<JSRegExp> New(
      Isolate* isolate, Handle<String> source, Flags flags,
      uint32_t backtrack_limit = kNoBacktrackLimit);
  static Handle<JSRegExp> Copy(Handle<JSRegExp> regexp);

  static MaybeHandle<JSRegExp> Initialize(
      Handle<JSRegExp> regexp, Handle<String> source, Flags flags,
      uint32_t backtrack_limit = kNoBacktrackLimit);
  static MaybeHandle<JSRegExp> Initialize(Handle<JSRegExp> regexp,
                                          Handle<String> source,
                                          Handle<String> flags_string);

  static Flags FlagsFromString(Isolate* isolate, Handle<String> flags,
                               bool* success);

  bool MarkedForTierUp();
  void ResetLastTierUpTick();
  void TierUpTick();
  void MarkTierUpForNextExec();

  inline Type TypeTag() const;

  // Maximum number of captures allowed.
  static constexpr int kMaxCaptures = 1 << 16;

  // Number of captures (without the match itself).
  inline int CaptureCount() const;
  // Each capture (including the match itself) needs two registers.
  static int RegistersForCaptureCount(int count) { return (count + 1) * 2; }

  inline int MaxRegisterCount() const;
  inline Flags GetFlags();
  inline String Pattern();
  inline Object CaptureNameMap();
  inline Object DataAt(int index) const;
  // Set implementation data after the object has been prepared.
  inline void SetDataAt(int index, Object value);

  static constexpr int code_index(bool is_latin1) {
    return is_latin1 ? kIrregexpLatin1CodeIndex : kIrregexpUC16CodeIndex;
  }

  static constexpr int bytecode_index(bool is_latin1) {
    return is_latin1 ? kIrregexpLatin1BytecodeIndex
                     : kIrregexpUC16BytecodeIndex;
  }

  // This could be a Smi kUninitializedValue or Code.
  V8_EXPORT_PRIVATE Object Code(bool is_latin1) const;
  // This could be a Smi kUninitializedValue or ByteArray.
  V8_EXPORT_PRIVATE Object Bytecode(bool is_latin1) const;

  bool ShouldProduceBytecode();
  inline bool HasCompiledCode() const;
  inline void DiscardCompiledCodeForSerialization();

  uint32_t BacktrackLimit() const;

  // Dispatched behavior.
  DECL_PRINTER(JSRegExp)
  DECL_VERIFIER(JSRegExp)

  /* This is already an in-object field. */
  // TODO(v8:8944): improve handling of in-object fields
  static constexpr int kLastIndexOffset = kHeaderSize;

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

  // Irregexp compiled code or trampoline to interpreter for Latin1. If
  // compilation fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpLatin1CodeIndex = kDataIndex;
  // Irregexp compiled code or trampoline to interpreter for UC16.  If
  // compilation fails, this fields hold an exception object that should be
  // thrown if the regexp is used again.
  static const int kIrregexpUC16CodeIndex = kDataIndex + 1;
  // Bytecode to interpret the regexp for Latin1. Contains kUninitializedValue
  // if we haven't compiled the regexp yet, regexp are always compiled or if
  // tier-up has happened (i.e. when kIrregexpLatin1CodeIndex contains native
  // irregexp code).
  static const int kIrregexpLatin1BytecodeIndex = kDataIndex + 2;
  // Bytecode to interpret the regexp for UC16. Contains kUninitializedValue if
  // we haven't compiled the regxp yet, regexp are always compiled or if tier-up
  // has happened (i.e. when kIrregexpUC16CodeIndex contains native irregexp
  // code).
  static const int kIrregexpUC16BytecodeIndex = kDataIndex + 3;
  // Maximal number of registers used by either Latin1 or UC16.
  // Only used to check that there is enough stack space
  static const int kIrregexpMaxRegisterCountIndex = kDataIndex + 4;
  // Number of captures in the compiled regexp.
  static const int kIrregexpCaptureCountIndex = kDataIndex + 5;
  // Maps names of named capture groups (at indices 2i) to their corresponding
  // (1-based) capture group indices (at indices 2i + 1).
  static const int kIrregexpCaptureNameMapIndex = kDataIndex + 6;
  // Tier-up ticks are set to the value of the tier-up ticks flag. The value is
  // decremented on each execution of the bytecode, so that the tier-up
  // happens once the ticks reach zero.
  // This value is ignored if the regexp-tier-up flag isn't turned on.
  static const int kIrregexpTicksUntilTierUpIndex = kDataIndex + 7;
  // A smi containing either the backtracking limit or kNoBacktrackLimit.
  // TODO(jgruber): If needed, this limit could be packed into other fields
  // above to save space.
  static const int kIrregexpBacktrackLimit = kDataIndex + 8;
  static const int kIrregexpDataSize = kDataIndex + 9;

  // In-object fields.
  static const int kLastIndexFieldIndex = 0;
  static const int kInObjectFieldCount = 1;

  // Descriptor array index to important methods in the prototype.
  static const int kExecFunctionDescriptorIndex = 1;
  static const int kSymbolMatchFunctionDescriptorIndex = 13;
  static const int kSymbolMatchAllFunctionDescriptorIndex = 14;
  static const int kSymbolReplaceFunctionDescriptorIndex = 15;
  static const int kSymbolSearchFunctionDescriptorIndex = 16;
  static const int kSymbolSplitFunctionDescriptorIndex = 17;

  // The uninitialized value for a regexp code object.
  static const int kUninitializedValue = -1;

  // The heuristic value for the length of the subject string for which we
  // tier-up to the compiler immediately, instead of using the interpreter.
  static constexpr int kTierUpForSubjectLengthValue = 1000;

  TQ_OBJECT_CONSTRUCTORS(JSRegExp)
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
  DECL_CAST(JSRegExpResult)

  // TODO(joshualitt): We would like to add printers and verifiers to
  // JSRegExpResult, and maybe JSRegExpResultIndices, but both have the same
  // instance type as JSArray.

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSArray::kHeaderSize,
                                TORQUE_GENERATED_JS_REG_EXP_RESULT_FIELDS)

  static MaybeHandle<JSArray> GetAndCacheIndices(
      Isolate* isolate, Handle<JSRegExpResult> regexp_result);

  // Indices of in-object properties.
  static const int kIndexIndex = 0;
  static const int kInputIndex = 1;
  static const int kGroupsIndex = 2;

  // Private internal only fields.
  static const int kCachedIndicesOrRegExpIndex = 3;
  static const int kNamesIndex = 4;
  static const int kRegExpInputIndex = 5;
  static const int kRegExpLastIndex = 6;
  static const int kInObjectPropertyCount = 7;

  OBJECT_CONSTRUCTORS(JSRegExpResult, JSArray);
};

// JSRegExpResultIndices is just a JSArray with a specific initial map.
// This initial map adds in-object properties for "group"
// properties, as assigned by RegExp.prototype.exec, which allows
// faster creation of RegExp exec results.
// This class just holds constants used when creating the result.
// After creation the result must be treated as a JSArray in all regards.
class JSRegExpResultIndices : public JSArray {
 public:
  DECL_CAST(JSRegExpResultIndices)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(
      JSArray::kHeaderSize, TORQUE_GENERATED_JS_REG_EXP_RESULT_INDICES_FIELDS)

  static Handle<JSRegExpResultIndices> BuildIndices(
      Isolate* isolate, Handle<RegExpMatchInfo> match_info,
      Handle<Object> maybe_names);

  // Indices of in-object properties.
  static const int kGroupsIndex = 0;
  static const int kInObjectPropertyCount = 1;

  // Descriptor index of groups.
  static const int kGroupsDescriptorIndex = 1;

  OBJECT_CONSTRUCTORS(JSRegExpResultIndices, JSArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_H_
