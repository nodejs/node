// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_STRING_TSA_INL_H_
#define V8_BUILTINS_BUILTINS_STRING_TSA_INL_H_

#include <math.h>

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/turboshaft-builtins-assembler-inl.h"
#include "src/common/globals.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/string-view.h"
#include "src/compiler/turboshaft/typeswitch.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/objects/string.h"
#include "src/objects/tagged-field.h"

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Next>
class StringBuiltinsReducer : public Next {
 public:
  BUILTIN_REDUCER(StringBuiltins)

  void CopyStringCharacters(V<String> src_string, ConstOrV<WordPtr> src_begin,
                            String::Encoding src_encoding, V<String> dst_string,
                            ConstOrV<WordPtr> dst_begin,
                            String::Encoding dst_encoding,
                            ConstOrV<WordPtr> character_count) {
    bool src_one_byte = src_encoding == String::ONE_BYTE_ENCODING;
    bool dst_one_byte = dst_encoding == String::ONE_BYTE_ENCODING;
    __ CodeComment("CopyStringCharacters ",
                   src_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING",
                   " -> ",
                   dst_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING");

    const auto dst_rep = dst_one_byte ? MemoryRepresentation::Uint8()
                                      : MemoryRepresentation::Uint16();
    static_assert(OFFSET_OF_DATA_START(SeqOneByteString) ==
                  OFFSET_OF_DATA_START(SeqTwoByteString));
    const size_t data_offset = OFFSET_OF_DATA_START(SeqOneByteString);
    const int dst_stride = dst_one_byte ? 1 : 2;

    DisallowGarbageCollection no_gc;
    V<WordPtr> dst_begin_offset =
        __ WordPtrAdd(__ BitcastTaggedToWordPtr(dst_string),
                      __ WordPtrAdd(data_offset - kHeapObjectTag,
                                    __ WordPtrMul(dst_begin, dst_stride)));

    StringView src_view(no_gc, src_string, src_encoding, src_begin,
                        character_count);
    FOREACH(src_char, dst_offset,
            Zip(src_view, Sequence(dst_begin_offset, dst_stride))) {
#if DEBUG
      // Copying two-byte characters to one-byte is okay if callers have
      // checked that this loses no information.
      if (v8_flags.debug_code && !src_one_byte && dst_one_byte) {
        TSA_DCHECK(this, __ Uint32LessThanOrEqual(src_char, 0xFF));
      }
#endif
      __ Store(dst_offset, src_char, StoreOp::Kind::RawAligned(), dst_rep,
               compiler::kNoWriteBarrier);
    }
  }

  V<SeqOneByteString> AllocateSeqOneByteString(V<WordPtr> length) {
    __ CodeComment("AllocateSeqOneByteString");
    Label<SeqOneByteString> done(this);
    GOTO_IF(__ WordPtrEqual(length, 0), done,
            V<SeqOneByteString>::Cast(__ EmptyStringConstant()));

    V<WordPtr> object_size =
        __ WordPtrAdd(sizeof(SeqOneByteString),
                      __ WordPtrMul(length, sizeof(SeqOneByteString::Char)));
    V<WordPtr> aligned_size = __ AlignTagged(object_size);
    Uninitialized<SeqOneByteString> new_string =
        __ template Allocate<SeqOneByteString>(
            aligned_size, AllocationType::kYoung, kTaggedAligned);
    __ InitializeField(new_string, AccessBuilderTS::ForMap(),
                       __ SeqOneByteStringMapConstant());

    __ InitializeField(new_string, AccessBuilderTS::ForStringLength(),
                       __ TruncateWordPtrToWord32(length));
    __ InitializeField(new_string, AccessBuilderTS::ForNameRawHashField(),
                       Name::kEmptyHashField);
    V<SeqOneByteString> string = __ FinishInitialization(std::move(new_string));
    // Clear padding.
    V<WordPtr> raw_padding_begin = __ WordPtrAdd(
        __ WordPtrAdd(__ BitcastTaggedToWordPtr(string), aligned_size),
        -kObjectAlignment - kHeapObjectTag);
    static_assert(kObjectAlignment ==
                  MemoryRepresentation::TaggedSigned().SizeInBytes());
    __ Store(raw_padding_begin, {}, __ SmiConstant(Smi::zero()),
             StoreOp::Kind::RawAligned(), MemoryRepresentation::TaggedSigned(),
             compiler::kNoWriteBarrier, 0, 0, true);
    GOTO(done, string);

    BIND(done, result);
    return result;
  }

  V<SeqTwoByteString> AllocateSeqTwoByteString(V<WordPtr> length) {
    __ CodeComment("AllocateSeqTwoByteString");
    Label<SeqTwoByteString> done(this);
    GOTO_IF(__ WordPtrEqual(length, 0), done,
            V<SeqTwoByteString>::Cast(__ EmptyStringConstant()));

    V<WordPtr> object_size =
        __ WordPtrAdd(sizeof(SeqTwoByteString),
                      __ WordPtrMul(length, sizeof(SeqTwoByteString::Char)));
    V<WordPtr> aligned_size = __ AlignTagged(object_size);
    Uninitialized<SeqTwoByteString> new_string =
        __ template Allocate<SeqTwoByteString>(
            aligned_size, AllocationType::kYoung, kTaggedAligned);
    __ InitializeField(new_string, AccessBuilderTS::ForMap(),
                       __ SeqTwoByteStringMapConstant());

    __ InitializeField(new_string, AccessBuilderTS::ForStringLength(),
                       __ TruncateWordPtrToWord32(length));
    __ InitializeField(new_string, AccessBuilderTS::ForNameRawHashField(),
                       Name::kEmptyHashField);
    V<SeqTwoByteString> string = __ FinishInitialization(std::move(new_string));
    // Clear padding.
    V<WordPtr> raw_padding_begin = __ WordPtrAdd(
        __ WordPtrAdd(__ BitcastTaggedToWordPtr(string), aligned_size),
        -kObjectAlignment - kHeapObjectTag);
    static_assert(kObjectAlignment ==
                  MemoryRepresentation::TaggedSigned().SizeInBytes());
    __ Store(raw_padding_begin, {}, __ SmiConstant(Smi::zero()),
             StoreOp::Kind::RawAligned(), MemoryRepresentation::TaggedSigned(),
             compiler::kNoWriteBarrier, 0, 0, true);
    GOTO(done, string);

    BIND(done, result);
    return result;
  }

  V<String> ToStringImpl(V<Context> context, V<JSAny> o) {
    ScopedVar<JSAny> result(this, o);
    Label<String> done(this);

    WHILE(1) {
      TYPESWITCH(result.Get()) {
        CASE_(V<Number>, num): {
          GOTO(done, __ NumberToString(num));
        }
        CASE_(V<String>, str): {
          GOTO(done, str);
        }
        CASE_(V<Oddball>, oddball): {
          GOTO(done, __ LoadField(oddball, FIELD(Oddball, to_string_)));
        }
        CASE_(V<JSReceiver>, receiver): {
          result = NonPrimitiveToPrimitive_String_Inline(context, receiver);
          CONTINUE;
        }
        CASE_(V<Symbol>, _): {
          __ ThrowTypeError(context, MessageTemplate::kSymbolToString);
        }
        CASE_(V<JSAny>, _): {
          GOTO(done, __ template CallRuntime<runtime::ToString>(
                         context, {.input = result}));
        }
      }
    }

    __ Unreachable();

    BIND(done, return_value);
    return return_value;
  }

  void GotoIfForceSlowPath(Label<>& if_true) {
    // TODO(nicohartmann): Provide this.
  }

  V<String> SmiToString(V<Smi> smi, Label<>& bailout) {
    // TODO(nicohartmann): Implement.
    GOTO(bailout);
    return {};
  }

  V<String> Float64ToString(V<Float64> f64, Label<>& bailout) {
    // TODO(nicohartmann): Implement.
    GOTO(bailout);
    return {};
  }

  V<Smi> TryFloat64ToSmi(V<Float64> f64, Label<>& if_not_smi) {
    // TODO(nicohartmann): Implement.
    GOTO(if_not_smi);
    return {};
  }

  V<String> NumberToString(V<Number> input, Label<>& bailout) {
    Label<String> done(this);
    Label<Smi> if_smi(this);
    Label<> if_not_smi(this);

    GOTO_IF(__ IsSmi(input), if_smi, V<Smi>::Cast(input));

    V<Float64> float64_input =
        __ LoadHeapNumberValue(V<HeapNumber>::Cast(input));
    __ Comment("NumberToString - HeapNumber");
    // Try normalizing the HeapNumber.
    V<Smi> smi_input = TryFloat64ToSmi(float64_input, if_not_smi);
    GOTO(if_smi, smi_input);

    BIND(if_smi, smi_value);
    GOTO(done, SmiToString(smi_input, bailout));

    BIND(if_not_smi);
    GOTO(done, Float64ToString(float64_input, bailout));

    BIND(done, return_value);
    return return_value;
  }

  V<String> NumberToString(V<Number> input) {
    Label<String> done(this);
    Label<> runtime(this);

    GotoIfForceSlowPath(runtime);

    V<String> result = NumberToString(input, runtime);
    GOTO(done, result);

    BIND(runtime);
    {
      // No cache entry, go to the runtime.
      GOTO(done, __ template CallRuntime<runtime::NumberToStringSlow>(
                     __ NoContextConstant(), {.input = input}));
    }

    BIND(done, return_value);
    return return_value;
  }

  V<NativeContext> LoadNativeContext(V<Context> context) {
    V<Map> map = __ LoadMapField(context);
    return __ LoadField(map, AccessBuilderTS::ForMapNativeContext());
  }

  V<Object> LoadNativeContextSlot(V<NativeContext> native_context, int slot) {
    // TODO(nicohartmann): Could use smi/known pointer specific.
    return __ LoadField(native_context, AccessBuilderTS::ForContextSlot(slot));
  }

  using JSArrayForFastToString = JSArray;
  V<JSArrayForFastToString> Cast_JSArrayForFastToString(V<Context> context,
                                                        V<HeapObject> o,
                                                        Label<>& cast_error) {
    GotoIfForceSlowPath(cast_error);

    // We are checking initial JSArray maps against current native context
    // because
    // a) it's faster to load it from current context,
    // b) in case array belongs to another native context the initial map check
    //    will just fail (we consider this a rare case).
    V<NativeContext> native_context = LoadNativeContext(context);

    V<Map> map = __ LoadMapField(o);
    V<Word32> elements_kind = __ LoadElementsKind(map);
    // TODO(ishell): add all JSArray initial maps to the native context.
    IF (__ IsFastElementsKind(elements_kind)) {
      // Faster check for fast JSArray cases (ensures both instance type is
      // JS_ARRAY_TYPE and prototype is the initial array prototype).
      V<Map> initial_array_map =
          __ LoadJSArrayElementsMap(elements_kind, native_context);
      GOTO_IF_NOT(__ TaggedEqual(map, initial_array_map), cast_error);
    } ELSE {
      // Check other JSArray cases (non-extensible elements kinds, dictionary
      // elements).

      // Verify that the prototype is the initial array prototype.
      V<Object> initial_array_prototype = LoadNativeContextSlot(
          native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
      V<HeapObject> prototype = __ template LoadField<HeapObject>(
          map, AccessBuilderTS::ForMapPrototype());
      GOTO_IF_NOT(__ TaggedEqual(prototype, initial_array_prototype),
                  cast_error);

      // Ensure it's a JSArray which doesn't have any properties except
      // "length".
      GOTO_IF_NOT(__ IsJSArrayMap(map), cast_error);
      V<Word32> map_bit_field3 = __ template LoadField<Word32>(
          map, AccessBuilderTS::ForMapBitField3());
      V<Word32> number_of_own_descriptors =
          __ template DecodeWord32<Map::Bits3::NumberOfOwnDescriptorsBits>(
              map_bit_field3);
      GOTO_IF_NOT(__ Word32Equal(number_of_own_descriptors, 1), cast_error);

      /* TODO(nicohartmann):
      dcheck(TaggedEqual(
          %RawDownCast<DescriptorArray>(map.instance_descriptors)
              .descriptors[0]
              .key,
          kLengthString));
      */
    }

    // Verify that the initial array prototype is not modified.
    V<Cell> validity_cell = V<Cell>::Cast(LoadNativeContextSlot(
        native_context, Context::INITIAL_ARRAY_PROTOTYPE_VALIDITY_CELL_INDEX));
    V<MaybeObject> maybe_value =
        __ LoadField(validity_cell, AccessBuilderTS::ForCellMaybeValue());
    GOTO_IF(__ TaggedEqual(maybe_value, __ PrototypeChainInvalidConstant()),
            cast_error);

    return V<JSArrayForFastToString>::Cast(o);
  }

  using ComparisonOp = compiler::turboshaft::ComparisonOp;
  // TODO(nicohartmann): Would be great if there was a way to reuse this to
  // compute the value and not just branch.
  void BranchIfNumberRelationalComparison(ComparisonOp::Kind op, V<Number> left,
                                          V<Number> right, Label<>* if_true,
                                          Label<>* if_false) {
    __ CodeComment("BranchIfNumberRelationalComparison");
    Label<Float64, Float64> do_float_comparison(this);

    IF (__ IsSmi(left)) {
      V<Smi> smi_left = V<Smi>::Cast(left);
      IF (__ IsSmi(right)) {
        V<Smi> smi_right = V<Smi>::Cast(right);

        // Both {left} and {right} are Smi, so just perform a fast
        // Smi comparison.
        switch (op) {
          case ComparisonOp::Kind::kEqual:
            GOTO_IF(__ SmiEqual(smi_left, smi_right), *if_true);
            GOTO(*if_false);
            break;
          case ComparisonOp::Kind::kSignedLessThan:
            GOTO_IF(__ SmiLessThan(smi_left, smi_right), *if_true);
            GOTO(*if_false);
            break;
          case ComparisonOp::Kind::kSignedLessThanOrEqual:
            GOTO_IF(__ SmiLessThanOrEqual(smi_left, smi_right), *if_true);
            GOTO(*if_false);
            break;
          default:
            UNREACHABLE();
        }
      } ELSE {
        GOTO(do_float_comparison, __ SmiToFloat64(smi_left),
             __ LoadHeapNumberValue(V<HeapNumber>::Cast(right)));
      }
    } ELSE {
      V<Float64> float_left = __ LoadHeapNumberValue(V<HeapNumber>::Cast(left));

      IF (__ IsSmi(right)) {
        GOTO(do_float_comparison, float_left,
             __ SmiToFloat64(V<Smi>::Cast(right)));
      } ELSE {
        GOTO(do_float_comparison, float_left,
             __ LoadHeapNumberValue(V<HeapNumber>::Cast(right)));
      }
    }

    BIND(do_float_comparison, f_left, f_right);
    GOTO_IF(
        __ Comparison(f_left, f_right, op, RegisterRepresentation::Float64()),
        *if_true);
    GOTO(*if_false);
  }

  V<Float64> SmiToFloat64(V<Smi> smi) {
    using ConvertJSPrimitiveToUntaggedOp =
        compiler::turboshaft::ConvertJSPrimitiveToUntaggedOp;
    return V<Float64>::Cast(__ ConvertJSPrimitiveToUntagged(
        smi, ConvertJSPrimitiveToUntaggedOp::UntaggedKind::kFloat64,
        ConvertJSPrimitiveToUntaggedOp::InputAssumptions::kSmi));
  }

  V<Word32> NumberComparison(ComparisonOp::Kind op, V<Number> left,
                             V<Number> right) {
    Label<> if_true(this), if_false(this);
    Label<Word32> done(this);

    BranchIfNumberRelationalComparison(op, left, right, &if_true, &if_false);
    BIND(if_true);
    GOTO(done, 1);

    BIND(if_false);
    GOTO(done, 0);

    BIND(done, result);
    return result;
  }

  V<Word32> NumberEqual(V<Number> left, V<Number> right) {
    return NumberComparison(ComparisonOp::Kind::kEqual, left, right);
  }

  V<Word32> NumberEqual(V<Number> left, ConstOrV<Smi> right) {
    return NumberComparison(ComparisonOp::Kind::kEqual, left,
                            __ resolve(right));
  }

  V<Word32> NumberLessThan(V<Number> left, V<Number> right) {
    return NumberComparison(ComparisonOp::Kind::kSignedLessThan, left, right);
  }

  V<Word32> NumberLessThan(ConstOrV<Smi> left, V<Number> right) {
    return NumberComparison(ComparisonOp::Kind::kSignedLessThan,
                            __ resolve(left), right);
  }

  V<String> ArrayPrototypeToString_Inline(V<Context> context,
                                          V<JSArrayForFastToString> array) {
    Label<String> done(this);
    Label<> if_fail(this);

    // 1. Let O be ? ToObject(this value).
    // 2. Let len be ? ToLength(? Get(O, "length")).
    V<Number> len = __ LoadField(array, AccessBuilderTS::ForJSArrayLength());

    GOTO_IF(NumberEqual(len, Smi::zero()), done, __ EmptyStringConstant());

    // Only handle valid array lengths. Although the spec allows larger
    // values, this matches historical V8 behavior.
    IF (UNLIKELY(
            NumberLessThan(__ NumberConstant(JSArray::kMaxArrayLength), len))) {
      __ ThrowTypeError(context, MessageTemplate::kInvalidArrayLength);
    }

    // 3. If separator is undefined, let sep be the single-element String ",".
    // 4. Else, let sep be ? ToString(separator).
    V<String> separator = __ StringConstant(",");

    V<JSAny> result = __ template CallBuiltin<builtin::ArrayPrototypeJoinImpl>(
        context, {.o = array, .len = len, .separator = separator});
    GOTO(done, __ template Cast<String>(result, &if_fail));

    BIND(if_fail);
    __ Unreachable();

    BIND(done, return_value);
    return return_value;
  }

  V<JSAny> TryGetExoticToPrimitive(V<Context> context, V<JSReceiver> input,
                                   Label<>* if_ordinary_to_primitive) {
    V<JSAny> exotic_to_primitive =
        GetInterestingProperty(context, input, __ ToPrimitiveSymbolConstant(),
                               if_ordinary_to_primitive);
    GOTO_IF(__ IsNull(exotic_to_primitive), *if_ordinary_to_primitive);
    GOTO_IF(__ IsUndefined(exotic_to_primitive), *if_ordinary_to_primitive);
    return exotic_to_primitive;
  }

  V<JSPrimitive> CallExoticToPrimitive(V<Context> context, V<JSAny> input,
                                       V<JSAny> exotic_to_primitive,
                                       V<String> hint) {
    Label<JSPrimitive> done(this);

    // Invoke the exoticToPrimitive method on the input with a string
    // representation of the hint.
    V<JSAny> result = __ Call(context, exotic_to_primitive, input, hint);

    // Verify that the result is primitive.
    TYPESWITCH(result) {
      CASE_(V<JSPrimitive>, o): {
        GOTO(done, o);
      }
      CASE_(V<JSReceiver>, _): {
        // Somehow the @@toPrimitive method on input didn't yield a primitive.
        __ ThrowTypeError(context, MessageTemplate::kCannotConvertToPrimitive);
      }
    }

    __ Unreachable();

    BIND(done, return_value);
    return return_value;
  }

  V<Word32> IsInterestingProperty(V<Name> name) {
    Label<Word32> done(this);
    // TODO(ishell): consider using ReadOnlyRoots::IsNameForProtector() trick
    // for these strings and interesting symbols.
    GOTO_IF(__ IsToJSONString(name), done, 1);
    GOTO_IF(__ IsGetString(name), done, 1);
    GOTO_IF_NOT(__ IsSymbolMap(__ LoadMapField(name)), done, 0);
    GOTO_IF(
        __ template IsSetWord32<Symbol::IsInterestingSymbolBit>(V<Word32>::Cast(
            __ LoadField(name, AccessBuilderTS::ForSymbolFlags()))),
        done, 1);
    GOTO(done, 0);

    BIND(done, result);
    return result;
  }

  V<Smi> GetNameDictionaryFlags(V<NameDictionary> dictionary) {
    return V<Smi>::Cast(
        __ LoadFixedArrayElement(dictionary, NameDictionary::kFlagsIndex));
  }

  V<Word32> HasInstanceType(V<HeapObject> heap_object,
                            InstanceType instance_type) {
#if V8_STATIC_ROOTS_BOOL
    if (std::optional<RootIndex> expected_map =
            InstanceTypeChecker::UniqueMapOfInstanceType(instance_type)) {
      V<Map> map = __ LoadMapField(heap_object);
      return __ TaggedEqual(map, __ LoadRoot(*expected_map));
    }
#endif
    return __ InstanceTypeEqual(
        __ LoadInstanceTypeField(__ LoadMapField(heap_object)), instance_type);
  }

  V<Word32> IsPropertyDictionary(V<HeapObject> heap_object) {
    return HasInstanceType(heap_object, PROPERTY_DICTIONARY_TYPE);
  }

  V<JSAny> GetInterestingProperty(V<Context> context, V<JSReceiver> receiver,
                                  V<Name> name, Label<>* if_not_found) {
    Label<> lookup(this);

    TSA_DCHECK(this, IsInterestingProperty(name));
    // The lookup starts at the var_holder and var_holder_map must contain
    // var_holder's map.
    ScopedVar<JSAnyNotSmi> var_holder(this, receiver);
    ScopedVar<Map> var_holder_map(this, __ LoadMapField(receiver));

    // Check if all relevant maps (including the prototype maps) don't
    // have any interesting properties (i.e. that none of them have the
    // @@toStringTag or @@toPrimitive property).
    WHILE(1) {
      GOTO_IF(__ IsNull(var_holder), *if_not_found);
      V<Word32> holder_bit_field3 = __ template LoadField<Word32>(
          var_holder_map, AccessBuilderTS::ForMapBitField3());
      IF (__ template IsSetWord32<Map::Bits3::MayHaveInterestingPropertiesBit>(
              holder_bit_field3)) {
        // Check flags for dictionary objects.
        GOTO_IF(__ template IsNotSetWord32<Map::Bits3::IsDictionaryMapBit>(
                    holder_bit_field3),
                lookup);
        // JSProxy has dictionary properties but has to be handled in runtime.
        GOTO_IF(__ InstanceTypeEqual(__ LoadInstanceTypeField(var_holder_map),
                                     JS_PROXY_TYPE),
                lookup);
        V<Object> properties = __ LoadField(
            var_holder.Get(), AccessBuilderTS::ForJSObjectPropertiesOrHash());

        TSA_DCHECK(this, __ IsNotSmi(properties));
        TSA_DCHECK(this, IsPropertyDictionary(V<HeapObject>::Cast(properties)));
        // TODO(pthier): Support swiss dictionaries.
        if constexpr (!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
          V<Smi> flags =
              GetNameDictionaryFlags(V<NameDictionary>::Cast(properties));
          GOTO_IF(__ IsSetSmi(
                      flags,
                      NameDictionary::MayHaveInterestingPropertiesBit::kMask),
                  lookup);
        }
      }
      var_holder = __ LoadField(var_holder_map.Get(),
                                AccessBuilderTS::ForMapPrototype());
      var_holder_map = __ LoadMapField(var_holder);
    }

    __ Unreachable();

    BIND(lookup);
    return __ template CallBuiltin<builtin::GetPropertyWithReceiver>(
        context, {.object = var_holder.Get(),
                  .key = name,
                  .receiver = receiver,
                  .on_non_existent = __ SmiConstant(
                      Smi::FromEnum(OnNonExistent::kReturnUndefined))});
  }

  // TODO(nicohartmann): Move to conversion.
  V<JSPrimitive> NonPrimitiveToPrimitive_String_Inline(V<Context> context,
                                                       V<JSReceiver> input) {
    Label<JSPrimitive> done(this);
    Label<> if_non_jsarray(this);
    Label<> if_ordinary(this);
    // If it's a JSArray without properties and with unmodified prototype
    // chain then
    //  - we can skip lookup for @@toPrimitive - it'll not succeed anyway,
    //  - we can skip lookup for toString - we know it'll be the
    //    Array.prototype.toString,
    //  - we can skip lookup for "join" property (done by toString()), we know
    //    it'll be the Array.prototype.join,
    // and thus we can short-cut directly to the middle of Array.prototype.join
    // implementation.
    auto array = Cast_JSArrayForFastToString(context, input, if_non_jsarray);
    GOTO(done, ArrayPrototypeToString_Inline(context, array));

    BIND(if_non_jsarray);
    V<JSAny> exotic_to_primitive =
        TryGetExoticToPrimitive(context, input, &if_ordinary);
    GOTO(done, CallExoticToPrimitive(context, input, exotic_to_primitive,
                                     __ StringStringConstant()));

    BIND(if_ordinary);
    GOTO(done, OrdinaryToPrimitive_String_Inline(context, input));

    BIND(done, return_value);
    return return_value;
  }

  V<JSPrimitive> OrdinaryToPrimitive_String_Inline(V<Context> context,
                                                   V<JSAny> input) {
    Label<> if_to_string_fail(this);
    Label<> if_value_of_fail(this);
    Label<JSPrimitive> done(this);

    {
      V<JSPrimitive> p = TryToPrimitiveMethod(
          context, input, __ ToStringStringConstant(), &if_to_string_fail);
      GOTO(done, p);
    }

    BIND(if_to_string_fail);
    {
      V<JSPrimitive> p = TryToPrimitiveMethod(
          context, input, __ ValueOfStringConstant(), &if_value_of_fail);
      GOTO(done, p);
    }

    BIND(if_value_of_fail);
    __ ThrowTypeError(context, MessageTemplate::kCannotConvertToPrimitive);

    BIND(done, result);
    return result;
  }

  V<JSAny> GetProperty(V<Context> context, V<JSAny> receiver, V<Object> name) {
    return __ template CallBuiltin<builtin::GetProperty>(
        context, {.object = receiver, .key = name});
  }

  V<JSPrimitive> TryToPrimitiveMethod(V<Context> context, V<JSAny> input,
                                      V<String> name, Label<>* if_fail) {
    DCHECK_NOT_NULL(if_fail);
    Label<JSPrimitive> done(this);

    V<JSAny> method = GetProperty(context, input, name);
    TYPESWITCH(method) {
      CASE_(V<JSCallable>, _): {
        V<JSAny> value = __ Call(context, method, input);
        V<JSPrimitive> p = __ template Cast<JSPrimitive>(value, if_fail);
        GOTO(done, p);
      }
      CASE_(V<JSAny>, _): {
        GOTO(*if_fail);
      }
    }

    __ Unreachable();

    BIND(done, result);
    return result;
  }
};

template <typename Next>
using StringBuiltinsReducers = StringBuiltinsReducer<Next>;

class StringBuiltinsAssemblerTS
    : public TurboshaftBuiltinsAssembler<StringBuiltinsReducers,
                                         NoFeedbackCollectorReducer> {
 public:
  using Base = TurboshaftBuiltinsAssembler;

  StringBuiltinsAssemblerTS(compiler::turboshaft::PipelineData* data,
                            compiler::turboshaft::Graph& graph,
                            Zone* phase_zone)
      : Base(data, graph, phase_zone) {}
  using Base::Asm;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal

#endif  // V8_BUILTINS_BUILTINS_STRING_TSA_INL_H_
