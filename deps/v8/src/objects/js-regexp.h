// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_H_
#define V8_OBJECTS_JS_REGEXP_H_

#include <optional>

#include "include/v8-regexp.h"
#include "src/objects/contexts.h"
#include "src/objects/js-array.h"
#include "src/objects/trusted-object.h"
#include "src/regexp/regexp-flags.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

class RegExpData;

#include "torque-generated/src/objects/js-regexp-tq.inc"

class RegExpData;

// Regular expressions
V8_OBJECT class JSRegExp : public JSObject {
 public:
  DEFINE_TORQUE_GENERATED_JS_REG_EXP_FLAGS()

  V8_EXPORT_PRIVATE static MaybeDirectHandle<JSRegExp> New(
      Isolate* isolate, DirectHandle<String> original_source, Flags flags,
      uint32_t backtrack_limit = kNoBacktrackLimit);

  static MaybeDirectHandle<JSRegExp> Initialize(
      Isolate* isolate, DirectHandle<JSRegExp> regexp,
      DirectHandle<String> original_source, Flags flags,
      uint32_t backtrack_limit = kNoBacktrackLimit);
  static MaybeDirectHandle<JSRegExp> Initialize(
      Isolate* isolate, DirectHandle<JSRegExp> regexp,
      DirectHandle<String> original_source, DirectHandle<String> flags_string);

  DECL_ACCESSORS(last_index, Tagged<Object>)

  // Instance fields accessors.
  // Returns the escaped source as specced by
  // https://tc39.es/ecma262/#sec-escaperegexppattern.
  inline Tagged<String> source(IsolateForSandbox isolate) const;
  inline Flags flags() const;
  inline void set_flags(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_TRUSTED_POINTER_ACCESSORS(data, RegExpData)

  static constexpr Flag AsJSRegExpFlag(regexp::Flag f) {
    return static_cast<Flag>(f);
  }
  static constexpr Flags AsJSRegExpFlags(regexp::Flags f) {
    return Flags{static_cast<int>(f)};
  }
  static constexpr regexp::Flags AsRegExpFlags(Flags f) {
    return regexp::Flags{static_cast<int>(f)};
  }

  static std::optional<regexp::Flag> FlagFromChar(char c) {
    std::optional<regexp::Flag> f = regexp::TryFlagFromChar(c);
    if (!f.has_value()) return f;
    if (f.value() == regexp::Flag::kLinear &&
        !v8_flags.enable_experimental_regexp_engine) {
      return {};
    }
    return f;
  }

  static_assert(static_cast<int>(kNone) == v8::RegExp::kNone);
#define V(_, Camel, ...)                                                   \
  static_assert(static_cast<int>(Flag::k##Camel) == v8::RegExp::k##Camel); \
  static_assert(static_cast<int>(Flag::k##Camel) ==                        \
                static_cast<int>(regexp::Flag::k##Camel));
  REGEXP_FLAG_LIST(V)
#undef V
  static_assert(kFlagCount == v8::RegExp::kFlagCount);
  static_assert(kFlagCount == regexp::kFlagCount);

  static std::optional<Flags> FlagsFromString(Isolate* isolate,
                                              DirectHandle<String> flags);

  V8_EXPORT_PRIVATE static DirectHandle<String> StringFromFlags(
      Isolate* isolate, Flags flags);

  // Each capture (including the match itself) needs two registers.
  static constexpr int RegistersForCaptureCount(int count) {
    return (count + 1) * 2;
  }
  static constexpr int CaptureCountForRegisters(int register_count) {
    DCHECK_EQ(register_count % 2, 0);
    DCHECK_GE(register_count, 2);
    return (register_count - 2) / 2;
  }
  // ATOM regexps don't have captures.
  static constexpr int kAtomCaptureCount = 0;
  static constexpr int kAtomRegisterCount = 2;

  // Dispatched behavior.
  DECL_PRINTER(JSRegExp)
  DECL_VERIFIER(JSRegExp)

  /* This is already an in-object field. */
  // TODO(v8:8944): improve handling of in-object fields
  static const int kLastIndexOffset;
  static constexpr int kInObjectFieldCount = 1;

  // The initial value of the last_index field on a new JSRegExp instance.
  static constexpr int kInitialLastIndexValue = 0;

  // The actual object size including in-object fields.
  static const int kSize;
  static int Size() { return kSize; }

  // Descriptor array index to important methods in the prototype.
  static constexpr int kExecFunctionDescriptorIndex = 1;
  static constexpr int kSymbolMatchFunctionDescriptorIndex = 15;
  static constexpr int kSymbolMatchAllFunctionDescriptorIndex = 16;
  static constexpr int kSymbolReplaceFunctionDescriptorIndex = 17;
  static constexpr int kSymbolSearchFunctionDescriptorIndex = 18;
  static constexpr int kSymbolSplitFunctionDescriptorIndex = 19;

  // The uninitialized value for a regexp code object.
  static constexpr int kUninitializedValue = -1;

  // If the backtrack limit is set to this marker value, no limit is applied.
  static constexpr uint32_t kNoBacktrackLimit = 0;

  // The heuristic value for the length of the subject string for which we
  // tier-up to the compiler immediately, instead of using the interpreter.
  static constexpr int kTierUpForSubjectLengthValue = 1000;

  // Maximum number of captures allowed.
  static constexpr int kMaxCaptures = 1 << 16;

  using FlagsBuffer = base::EmbeddedVector<char, kFlagCount + 1>;
  inline static const char* FlagsToString(Flags flags, FlagsBuffer* out_buffer);

  class BodyDescriptor;

  // Back-compat offset / size constants.
  static const int kDataOffset;
  static const int kFlagsOffset;
  static const int kHeaderSize;

 private:
  friend class RegExpData;
  friend class TorqueGeneratedJSRegExpAsserts;

 public:
  TrustedPointerMember<RegExpData, kRegExpDataIndirectPointerTag> data_;
  TaggedMember<Object> flags_;
} V8_OBJECT_END;

inline constexpr int JSRegExp::kDataOffset = offsetof(JSRegExp, data_);
inline constexpr int JSRegExp::kFlagsOffset = offsetof(JSRegExp, flags_);
inline constexpr int JSRegExp::kHeaderSize = sizeof(JSRegExp);
inline constexpr int JSRegExp::kLastIndexOffset = kHeaderSize;
inline constexpr int JSRegExp::kSize =
    kHeaderSize + kInObjectFieldCount * kTaggedSize;

DEFINE_OPERATORS_FOR_FLAGS(JSRegExp::Flags)

class RegExpDataWrapper;

V8_OBJECT class RegExpData : public ExposedTrustedObject {
 public:
  enum class Type : uint8_t {
    ATOM,          // A simple string match.
    IRREGEXP,      // Compiled with Irregexp (code or bytecode).
    EXPERIMENTAL,  // Compiled to use the experimental linear time engine.
  };

  inline Type type_tag() const;
  inline void set_type_tag(Type);

  inline Tagged<String> original_source() const;
  inline void set_original_source(Tagged<String> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> escaped_source() const;
  inline void set_escaped_source(Tagged<String> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline JSRegExp::Flags flags() const;
  inline void set_flags(JSRegExp::Flags flags);

  inline Tagged<RegExpDataWrapper> wrapper() const;
  inline void set_wrapper(Tagged<RegExpDataWrapper> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline uint32_t quick_check_mask() const;
  inline void set_quick_check_mask(uint32_t value);

  inline uint32_t quick_check_value() const;
  inline void set_quick_check_value(uint32_t value);

  inline int capture_count() const;

  static constexpr bool TypeSupportsCaptures(Type t) {
    return t == Type::IRREGEXP || t == Type::EXPERIMENTAL;
  }

  V8_EXPORT_PRIVATE bool HasCompiledCode() const;

  DECL_PRINTER(RegExpData)
  DECL_VERIFIER(RegExpData)

  class BodyDescriptor;

  TaggedMember<Smi> type_tag_;
  TaggedMember<String> original_source_;
  TaggedMember<String> escaped_source_;
  TaggedMember<Smi> flags_;
  TaggedMember<RegExpDataWrapper> wrapper_;
  uint32_t quick_check_mask_;
  uint32_t quick_check_value_;
} V8_OBJECT_END;

V8_OBJECT class RegExpDataWrapper : public Struct {
 public:
  DECL_TRUSTED_POINTER_ACCESSORS(data, RegExpData)

  DECL_PRINTER(RegExpDataWrapper)
  DECL_VERIFIER(RegExpDataWrapper)

  class BodyDescriptor;

  TrustedPointerMember<RegExpData, kRegExpDataIndirectPointerTag> data_;
} V8_OBJECT_END;

V8_OBJECT class AtomRegExpData : public RegExpData {
 public:
  inline Tagged<String> pattern() const;
  inline void set_pattern(Tagged<String> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(AtomRegExpData)
  DECL_VERIFIER(AtomRegExpData)

  class BodyDescriptor;

  TaggedMember<String> pattern_;
} V8_OBJECT_END;

V8_OBJECT class IrRegExpData : public RegExpData {
 public:
  DECL_CODE_POINTER_ACCESSORS(latin1_code)
  DECL_CODE_POINTER_ACCESSORS(uc16_code)
  inline bool has_code(bool is_one_byte) const;
  inline void set_code(bool is_one_byte, Tagged<Code> code);
  inline Tagged<Code> code(IsolateForSandbox isolate, bool is_one_byte) const;
  DECL_PROTECTED_POINTER_ACCESSORS(latin1_bytecode, TrustedByteArray)
  DECL_PROTECTED_POINTER_ACCESSORS(uc16_bytecode, TrustedByteArray)
  DECL_PROTECTED_POINTER_ACCESSORS(capture_name_map, TrustedFixedArray)
  inline void set_capture_name_map(DirectHandle<TrustedFixedArray> value);
  inline bool has_bytecode(bool is_one_byte) const;
  inline void clear_bytecode(bool is_one_byte);
  inline void set_bytecode(bool is_one_byte, Tagged<TrustedByteArray> bytecode);
  inline Tagged<TrustedByteArray> bytecode(bool is_one_byte) const;
  inline int max_register_count() const;
  inline void set_max_register_count(int value);
  // Number of captures (without the match itself).
  inline int capture_count() const;
  inline void set_capture_count(int value);
  inline int ticks_until_tier_up() const;
  inline void set_ticks_until_tier_up(int value);
  inline int backtrack_limit() const;
  inline void set_backtrack_limit(int value);

  inline uint32_t bit_field() const;
  inline void set_bit_field(uint32_t value);
  DECL_BOOLEAN_ACCESSORS(can_be_zero_length)
  DECL_BOOLEAN_ACCESSORS(is_linear_executable)

  struct Bits {
    DEFINE_TORQUE_GENERATED_IR_REG_EXP_DATA_BIT_FIELD()
  };

  bool CanTierUp();
  bool MarkedForTierUp();
  void ResetLastTierUpTick();
  void TierUpTick();
  void MarkTierUpForNextExec();
  bool ShouldProduceBytecode();

  void DiscardCompiledCodeForSerialization();

  // Sets the bytecode as well as initializing trampoline slots to the
  // RegExpExperimentalTrampoline.
  void SetBytecodeForExperimental(Isolate* isolate,
                                  Tagged<TrustedByteArray> bytecode);

  DECL_PRINTER(IrRegExpData)
  DECL_VERIFIER(IrRegExpData)

  class BodyDescriptor;

  ProtectedTaggedMember<TrustedByteArray> latin1_bytecode_;
  ProtectedTaggedMember<TrustedByteArray> uc16_bytecode_;
  ProtectedTaggedMember<TrustedFixedArray> capture_name_map_;
  CodePointerMember latin1_code_;
  CodePointerMember uc16_code_;
  TaggedMember<Smi> max_register_count_;
  TaggedMember<Smi> capture_count_;
  TaggedMember<Smi> ticks_until_tier_up_;
  TaggedMember<Smi> backtrack_limit_;
  TaggedMember<Smi> bit_field_;
} V8_OBJECT_END;

// JSRegExpResult is just a JSArray with a specific initial map.
// This initial map adds in-object properties for "index" and "input"
// properties, as assigned by RegExp.prototype.exec, which allows
// faster creation of RegExp exec results.
// This class just holds constants used when creating the result.
// After creation the result must be treated as a JSArray in all regards.
// JSRegExpResult is a JSArray with in-object properties for exec() results.
// Shape-style class: no own C++ storage. Fields live in the parent JSArray's
// in-object property slots at fixed offsets past JSArray::kHeaderSize. Matches
// the layout that legacy Torque-generated for the `extern shape` declaration
// in js-regexp.tq: `kHeaderSize` is inherited from JSArray (no new header),
// and `kInObjectPropertyCount` is the total count of in-object properties
// relative to JSArray::kHeaderSize.
V8_OBJECT class JSRegExpResult : public JSArray {
 public:
  // TODO(joshualitt): We would like to add printers and verifiers to
  // JSRegExpResult, and maybe JSRegExpResultIndices, but both have the same
  // instance type as JSArray.

  // Slot indices of the in-object properties, relative to JSArray::kHeaderSize.
  static constexpr int kIndexSlotIndex = 0;
  static constexpr int kInputSlotIndex = 1;
  static constexpr int kGroupsSlotIndex = 2;
  static constexpr int kRegexpInputSlotIndex = 3;
  static constexpr int kRegexpLastIndexSlotIndex = 4;
  static constexpr int kInObjectPropertyCount = 5;

  static constexpr int kIndexOffset =
      JSArray::kHeaderSize + kIndexSlotIndex * kTaggedSize;
  static constexpr int kInputOffset =
      JSArray::kHeaderSize + kInputSlotIndex * kTaggedSize;
  static constexpr int kGroupsOffset =
      JSArray::kHeaderSize + kGroupsSlotIndex * kTaggedSize;
  static constexpr int kRegexpInputOffset =
      JSArray::kHeaderSize + kRegexpInputSlotIndex * kTaggedSize;
  static constexpr int kRegexpLastIndexOffset =
      JSArray::kHeaderSize + kRegexpLastIndexSlotIndex * kTaggedSize;
  static constexpr int kSize =
      JSArray::kHeaderSize + kInObjectPropertyCount * kTaggedSize;

  static constexpr int kMapIndexInContext = Context::REGEXP_RESULT_MAP_INDEX;
} V8_OBJECT_END;

V8_OBJECT class JSRegExpResultWithIndices : public JSRegExpResult {
 public:
  // Additional in-object property past JSRegExpResult's fields.
  static constexpr int kAdditionalInObjectPropertyCount = 1;
  static constexpr int kIndicesOffset = JSRegExpResult::kSize;
  static constexpr int kSize =
      JSRegExpResult::kSize + kAdditionalInObjectPropertyCount * kTaggedSize;
  // Total in-object property count relative to JSArray::kHeaderSize.
  static constexpr int kInObjectPropertyCount =
      JSRegExpResult::kInObjectPropertyCount + kAdditionalInObjectPropertyCount;
} V8_OBJECT_END;

// JSRegExpResultIndices is just a JSArray with a specific initial map.
// This initial map adds in-object properties for "group"
// properties, as assigned by RegExp.prototype.exec, which allows
// faster creation of RegExp exec results.
// This class just holds constants used when creating the result.
// After creation the result must be treated as a JSArray in all regards.
V8_OBJECT class JSRegExpResultIndices : public JSArray {
 public:
  static DirectHandle<JSRegExpResultIndices> BuildIndices(
      Isolate* isolate, DirectHandle<RegExpMatchInfo> match_info,
      DirectHandle<RegExpData> re_data);

  static constexpr int kGroupsSlotIndex = 0;
  static constexpr int kInObjectPropertyCount = 1;

  static constexpr int kGroupsOffset =
      JSArray::kHeaderSize + kGroupsSlotIndex * kTaggedSize;
  static constexpr int kSize =
      JSArray::kHeaderSize + kInObjectPropertyCount * kTaggedSize;

  // Descriptor index of groups.
  static constexpr int kGroupsDescriptorIndex = 1;
} V8_OBJECT_END;

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_H_
