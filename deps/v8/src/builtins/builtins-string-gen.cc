// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-string-gen.h"

#include "src/builtins/builtins-regexp-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/execution/protectors.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/objects.h"
#include "src/objects/property-cell.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;

TNode<IntPtrT> StringBuiltinsAssembler::DirectStringData(
    TNode<String> string, TNode<Word32T> string_instance_type) {
  // Compute the effective offset of the first character.
  TVARIABLE(IntPtrT, var_data);
  Label if_sequential(this), if_external(this), if_join(this);
  Branch(Word32Equal(Word32And(string_instance_type,
                               Int32Constant(kStringRepresentationMask)),
                     Int32Constant(kSeqStringTag)),
         &if_sequential, &if_external);

  BIND(&if_sequential);
  {
    var_data = IntPtrAdd(
        IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag),
        BitcastTaggedToWord(string));
    Goto(&if_join);
  }

  BIND(&if_external);
  {
    // This is only valid for ExternalStrings where the resource data
    // pointer is cached (i.e. no uncached external strings).
    CSA_ASSERT(this, Word32NotEqual(
                         Word32And(string_instance_type,
                                   Int32Constant(kUncachedExternalStringMask)),
                         Int32Constant(kUncachedExternalStringTag)));
    var_data =
        LoadObjectField<IntPtrT>(string, ExternalString::kResourceDataOffset);
    Goto(&if_join);
  }

  BIND(&if_join);
  return var_data.value();
}

void StringBuiltinsAssembler::DispatchOnStringEncodings(
    TNode<Word32T> const lhs_instance_type,
    TNode<Word32T> const rhs_instance_type, Label* if_one_one,
    Label* if_one_two, Label* if_two_one, Label* if_two_two) {
  STATIC_ASSERT(kStringEncodingMask == 0x8);
  STATIC_ASSERT(kTwoByteStringTag == 0x0);
  STATIC_ASSERT(kOneByteStringTag == 0x8);

  // First combine the encodings.

  const TNode<Int32T> encoding_mask = Int32Constant(kStringEncodingMask);
  const TNode<Word32T> lhs_encoding =
      Word32And(lhs_instance_type, encoding_mask);
  const TNode<Word32T> rhs_encoding =
      Word32And(rhs_instance_type, encoding_mask);

  const TNode<Word32T> combined_encodings =
      Word32Or(lhs_encoding, Word32Shr(rhs_encoding, 1));

  // Then dispatch on the combined encoding.

  Label unreachable(this, Label::kDeferred);

  int32_t values[] = {
      kOneByteStringTag | (kOneByteStringTag >> 1),
      kOneByteStringTag | (kTwoByteStringTag >> 1),
      kTwoByteStringTag | (kOneByteStringTag >> 1),
      kTwoByteStringTag | (kTwoByteStringTag >> 1),
  };
  Label* labels[] = {
      if_one_one, if_one_two, if_two_one, if_two_two,
  };

  STATIC_ASSERT(arraysize(values) == arraysize(labels));
  Switch(combined_encodings, &unreachable, values, labels, arraysize(values));

  BIND(&unreachable);
  Unreachable();
}

template <typename SubjectChar, typename PatternChar>
TNode<IntPtrT> StringBuiltinsAssembler::CallSearchStringRaw(
    const TNode<RawPtrT> subject_ptr, const TNode<IntPtrT> subject_length,
    const TNode<RawPtrT> search_ptr, const TNode<IntPtrT> search_length,
    const TNode<IntPtrT> start_position) {
  const TNode<ExternalReference> function_addr = ExternalConstant(
      ExternalReference::search_string_raw<SubjectChar, PatternChar>());
  const TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_intptr = MachineType::IntPtr();

  const TNode<IntPtrT> result = UncheckedCast<IntPtrT>(CallCFunction(
      function_addr, type_intptr, std::make_pair(type_ptr, isolate_ptr),
      std::make_pair(type_ptr, subject_ptr),
      std::make_pair(type_intptr, subject_length),
      std::make_pair(type_ptr, search_ptr),
      std::make_pair(type_intptr, search_length),
      std::make_pair(type_intptr, start_position)));

  return result;
}

TNode<RawPtrT> StringBuiltinsAssembler::PointerToStringDataAtIndex(
    TNode<RawPtrT> string_data, TNode<IntPtrT> index,
    String::Encoding encoding) {
  const ElementsKind kind = (encoding == String::ONE_BYTE_ENCODING)
                                ? UINT8_ELEMENTS
                                : UINT16_ELEMENTS;
  TNode<IntPtrT> offset_in_bytes = ElementOffsetFromIndex(index, kind);
  return RawPtrAdd(string_data, offset_in_bytes);
}

void StringBuiltinsAssembler::GenerateStringEqual(TNode<String> left,
                                                  TNode<String> right) {
  TVARIABLE(String, var_left, left);
  TVARIABLE(String, var_right, right);
  Label if_equal(this), if_notequal(this), if_indirect(this, Label::kDeferred),
      restart(this, {&var_left, &var_right});

  TNode<IntPtrT> lhs_length = LoadStringLengthAsWord(left);
  TNode<IntPtrT> rhs_length = LoadStringLengthAsWord(right);

  // Strings with different lengths cannot be equal.
  GotoIf(WordNotEqual(lhs_length, rhs_length), &if_notequal);

  Goto(&restart);
  BIND(&restart);
  TNode<String> lhs = var_left.value();
  TNode<String> rhs = var_right.value();

  TNode<Uint16T> lhs_instance_type = LoadInstanceType(lhs);
  TNode<Uint16T> rhs_instance_type = LoadInstanceType(rhs);

  StringEqual_Core(lhs, lhs_instance_type, rhs, rhs_instance_type, lhs_length,
                   &if_equal, &if_notequal, &if_indirect);

  BIND(&if_indirect);
  {
    // Try to unwrap indirect strings, restart the above attempt on success.
    MaybeDerefIndirectStrings(&var_left, lhs_instance_type, &var_right,
                              rhs_instance_type, &restart);

    TailCallRuntime(Runtime::kStringEqual, NoContextConstant(), lhs, rhs);
  }

  BIND(&if_equal);
  Return(TrueConstant());

  BIND(&if_notequal);
  Return(FalseConstant());
}

void StringBuiltinsAssembler::StringEqual_Core(
    TNode<String> lhs, TNode<Word32T> lhs_instance_type, TNode<String> rhs,
    TNode<Word32T> rhs_instance_type, TNode<IntPtrT> length, Label* if_equal,
    Label* if_not_equal, Label* if_indirect) {
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(lhs), length));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(rhs), length));
  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  GotoIf(TaggedEqual(lhs, rhs), if_equal);

  // Combine the instance types into a single 16-bit value, so we can check
  // both of them at once.
  TNode<Word32T> both_instance_types = Word32Or(
      lhs_instance_type, Word32Shl(rhs_instance_type, Int32Constant(8)));

  // Check if both {lhs} and {rhs} are internalized. Since we already know
  // that they're not the same object, they're not equal in that case.
  int const kBothInternalizedMask =
      kIsNotInternalizedMask | (kIsNotInternalizedMask << 8);
  int const kBothInternalizedTag = kInternalizedTag | (kInternalizedTag << 8);
  GotoIf(Word32Equal(Word32And(both_instance_types,
                               Int32Constant(kBothInternalizedMask)),
                     Int32Constant(kBothInternalizedTag)),
         if_not_equal);

  // Check if both {lhs} and {rhs} are direct strings, and that in case of
  // ExternalStrings the data pointer is cached.
  STATIC_ASSERT(kUncachedExternalStringTag != 0);
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  int const kBothDirectStringMask =
      kIsIndirectStringMask | kUncachedExternalStringMask |
      ((kIsIndirectStringMask | kUncachedExternalStringMask) << 8);
  GotoIfNot(Word32Equal(Word32And(both_instance_types,
                                  Int32Constant(kBothDirectStringMask)),
                        Int32Constant(0)),
            if_indirect);

  // Dispatch based on the {lhs} and {rhs} string encoding.
  int const kBothStringEncodingMask =
      kStringEncodingMask | (kStringEncodingMask << 8);
  int const kOneOneByteStringTag = kOneByteStringTag | (kOneByteStringTag << 8);
  int const kTwoTwoByteStringTag = kTwoByteStringTag | (kTwoByteStringTag << 8);
  int const kOneTwoByteStringTag = kOneByteStringTag | (kTwoByteStringTag << 8);
  Label if_oneonebytestring(this), if_twotwobytestring(this),
      if_onetwobytestring(this), if_twoonebytestring(this);
  TNode<Word32T> masked_instance_types =
      Word32And(both_instance_types, Int32Constant(kBothStringEncodingMask));
  GotoIf(
      Word32Equal(masked_instance_types, Int32Constant(kOneOneByteStringTag)),
      &if_oneonebytestring);
  GotoIf(
      Word32Equal(masked_instance_types, Int32Constant(kTwoTwoByteStringTag)),
      &if_twotwobytestring);
  Branch(
      Word32Equal(masked_instance_types, Int32Constant(kOneTwoByteStringTag)),
      &if_onetwobytestring, &if_twoonebytestring);

  BIND(&if_oneonebytestring);
  StringEqual_Loop(lhs, lhs_instance_type, MachineType::Uint8(), rhs,
                   rhs_instance_type, MachineType::Uint8(), length, if_equal,
                   if_not_equal);

  BIND(&if_twotwobytestring);
  StringEqual_Loop(lhs, lhs_instance_type, MachineType::Uint16(), rhs,
                   rhs_instance_type, MachineType::Uint16(), length, if_equal,
                   if_not_equal);

  BIND(&if_onetwobytestring);
  StringEqual_Loop(lhs, lhs_instance_type, MachineType::Uint8(), rhs,
                   rhs_instance_type, MachineType::Uint16(), length, if_equal,
                   if_not_equal);

  BIND(&if_twoonebytestring);
  StringEqual_Loop(lhs, lhs_instance_type, MachineType::Uint16(), rhs,
                   rhs_instance_type, MachineType::Uint8(), length, if_equal,
                   if_not_equal);
}

void StringBuiltinsAssembler::StringEqual_Loop(
    TNode<String> lhs, TNode<Word32T> lhs_instance_type, MachineType lhs_type,
    TNode<String> rhs, TNode<Word32T> rhs_instance_type, MachineType rhs_type,
    TNode<IntPtrT> length, Label* if_equal, Label* if_not_equal) {
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(lhs), length));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(rhs), length));

  // Compute the effective offset of the first character.
  TNode<IntPtrT> lhs_data = DirectStringData(lhs, lhs_instance_type);
  TNode<IntPtrT> rhs_data = DirectStringData(rhs, rhs_instance_type);

  // Loop over the {lhs} and {rhs} strings to see if they are equal.
  TVARIABLE(IntPtrT, var_offset, IntPtrConstant(0));
  Label loop(this, &var_offset);
  Goto(&loop);
  BIND(&loop);
  {
    // If {offset} equals {end}, no difference was found, so the
    // strings are equal.
    GotoIf(WordEqual(var_offset.value(), length), if_equal);

    // Load the next characters from {lhs} and {rhs}.
    TNode<Word32T> lhs_value = UncheckedCast<Word32T>(
        Load(lhs_type, lhs_data,
             WordShl(var_offset.value(),
                     ElementSizeLog2Of(lhs_type.representation()))));
    TNode<Word32T> rhs_value = UncheckedCast<Word32T>(
        Load(rhs_type, rhs_data,
             WordShl(var_offset.value(),
                     ElementSizeLog2Of(rhs_type.representation()))));

    // Check if the characters match.
    GotoIf(Word32NotEqual(lhs_value, rhs_value), if_not_equal);

    // Advance to next character.
    var_offset = IntPtrAdd(var_offset.value(), IntPtrConstant(1));
    Goto(&loop);
  }
}

TNode<String> StringBuiltinsAssembler::StringFromSingleUTF16EncodedCodePoint(
    TNode<Int32T> codepoint) {
  TVARIABLE(String, var_result, EmptyStringConstant());

  Label if_isword16(this), if_isword32(this), return_result(this);

  Branch(Uint32LessThan(codepoint, Int32Constant(0x10000)), &if_isword16,
         &if_isword32);

  BIND(&if_isword16);
  {
    var_result = StringFromSingleCharCode(codepoint);
    Goto(&return_result);
  }

  BIND(&if_isword32);
  {
    TNode<String> value = AllocateSeqTwoByteString(2);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, value,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        codepoint);
    var_result = value;
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

TNode<String> StringBuiltinsAssembler::AllocateConsString(TNode<Uint32T> length,
                                                          TNode<String> left,
                                                          TNode<String> right) {
  // Added string can be a cons string.
  Comment("Allocating ConsString");
  TNode<Int32T> left_instance_type = LoadInstanceType(left);
  TNode<Int32T> right_instance_type = LoadInstanceType(right);

  // Determine the resulting ConsString map to use depending on whether
  // any of {left} or {right} has two byte encoding.
  STATIC_ASSERT(kOneByteStringTag != 0);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  TNode<Int32T> combined_instance_type =
      Word32And(left_instance_type, right_instance_type);
  TNode<Map> result_map = CAST(Select<Object>(
      IsSetWord32(combined_instance_type, kStringEncodingMask),
      [=] { return ConsOneByteStringMapConstant(); },
      [=] { return ConsStringMapConstant(); }));
  TNode<HeapObject> result = AllocateInNewSpace(ConsString::kSize);
  StoreMapNoWriteBarrier(result, result_map);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kLengthOffset, length,
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kHashFieldOffset,
                                 Int32Constant(String::kEmptyHashField),
                                 MachineRepresentation::kWord32);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kFirstOffset, left);
  StoreObjectFieldNoWriteBarrier(result, ConsString::kSecondOffset, right);
  return CAST(result);
}

TNode<String> StringBuiltinsAssembler::StringAdd(SloppyTNode<Context> context,
                                                 TNode<String> left,
                                                 TNode<String> right) {
  TVARIABLE(String, result);
  Label check_right(this), runtime(this, Label::kDeferred), cons(this),
      done(this, &result), done_native(this, &result);
  Counters* counters = isolate()->counters();

  TNode<Uint32T> left_length = LoadStringLengthAsWord32(left);
  GotoIfNot(Word32Equal(left_length, Uint32Constant(0)), &check_right);
  result = right;
  Goto(&done_native);

  BIND(&check_right);
  TNode<Uint32T> right_length = LoadStringLengthAsWord32(right);
  GotoIfNot(Word32Equal(right_length, Uint32Constant(0)), &cons);
  result = left;
  Goto(&done_native);

  BIND(&cons);
  {
    TNode<Uint32T> new_length = Uint32Add(left_length, right_length);

    // If new length is greater than String::kMaxLength, goto runtime to
    // throw. Note: we also need to invalidate the string length protector, so
    // can't just throw here directly.
    GotoIf(Uint32GreaterThan(new_length, Uint32Constant(String::kMaxLength)),
           &runtime);

    TVARIABLE(String, var_left, left);
    TVARIABLE(String, var_right, right);
    Label non_cons(this, {&var_left, &var_right});
    Label slow(this, Label::kDeferred);
    GotoIf(Uint32LessThan(new_length, Uint32Constant(ConsString::kMinLength)),
           &non_cons);

    result =
        AllocateConsString(new_length, var_left.value(), var_right.value());
    Goto(&done_native);

    BIND(&non_cons);

    Comment("Full string concatenate");
    TNode<Int32T> left_instance_type = LoadInstanceType(var_left.value());
    TNode<Int32T> right_instance_type = LoadInstanceType(var_right.value());
    // Compute intersection and difference of instance types.

    TNode<Int32T> ored_instance_types =
        Word32Or(left_instance_type, right_instance_type);
    TNode<Word32T> xored_instance_types =
        Word32Xor(left_instance_type, right_instance_type);

    // Check if both strings have the same encoding and both are sequential.
    GotoIf(IsSetWord32(xored_instance_types, kStringEncodingMask), &runtime);
    GotoIf(IsSetWord32(ored_instance_types, kStringRepresentationMask), &slow);

    TNode<IntPtrT> word_left_length = Signed(ChangeUint32ToWord(left_length));
    TNode<IntPtrT> word_right_length = Signed(ChangeUint32ToWord(right_length));

    Label two_byte(this);
    GotoIf(Word32Equal(Word32And(ored_instance_types,
                                 Int32Constant(kStringEncodingMask)),
                       Int32Constant(kTwoByteStringTag)),
           &two_byte);
    // One-byte sequential string case
    result = AllocateSeqOneByteString(new_length);
    CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                         IntPtrConstant(0), word_left_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                         word_left_length, word_right_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    Goto(&done_native);

    BIND(&two_byte);
    {
      // Two-byte sequential string case
      result = AllocateSeqTwoByteString(new_length);
      CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                           IntPtrConstant(0), word_left_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                           word_left_length, word_right_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      Goto(&done_native);
    }

    BIND(&slow);
    {
      // Try to unwrap indirect strings, restart the above attempt on success.
      MaybeDerefIndirectStrings(&var_left, left_instance_type, &var_right,
                                right_instance_type, &non_cons);
      Goto(&runtime);
    }
  }
  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kStringAdd, context, left, right));
    Goto(&done);
  }

  BIND(&done_native);
  {
    IncrementCounter(counters->string_add_native(), 1);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

void StringBuiltinsAssembler::BranchIfCanDerefIndirectString(
    TNode<String> string, TNode<Int32T> instance_type, Label* can_deref,
    Label* cannot_deref) {
  TNode<Int32T> representation =
      Word32And(instance_type, Int32Constant(kStringRepresentationMask));
  GotoIf(Word32Equal(representation, Int32Constant(kThinStringTag)), can_deref);
  GotoIf(Word32NotEqual(representation, Int32Constant(kConsStringTag)),
         cannot_deref);
  // Cons string.
  TNode<String> rhs =
      LoadObjectField<String>(string, ConsString::kSecondOffset);
  GotoIf(IsEmptyString(rhs), can_deref);
  Goto(cannot_deref);
}

void StringBuiltinsAssembler::DerefIndirectString(TVariable<String>* var_string,
                                                  TNode<Int32T> instance_type) {
#ifdef DEBUG
  Label can_deref(this), cannot_deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &can_deref,
                                 &cannot_deref);
  BIND(&cannot_deref);
  DebugBreak();  // Should be able to dereference string.
  Goto(&can_deref);
  BIND(&can_deref);
#endif  // DEBUG

  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  *var_string =
      LoadObjectField<String>(var_string->value(), ThinString::kActualOffset);
}

void StringBuiltinsAssembler::MaybeDerefIndirectString(
    TVariable<String>* var_string, TNode<Int32T> instance_type,
    Label* did_deref, Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &deref,
                                 cannot_deref);

  BIND(&deref);
  {
    DerefIndirectString(var_string, instance_type);
    Goto(did_deref);
  }
}

void StringBuiltinsAssembler::MaybeDerefIndirectStrings(
    TVariable<String>* var_left, TNode<Int32T> left_instance_type,
    TVariable<String>* var_right, TNode<Int32T> right_instance_type,
    Label* did_something) {
  Label did_nothing_left(this), did_something_left(this),
      didnt_do_anything(this);
  MaybeDerefIndirectString(var_left, left_instance_type, &did_something_left,
                           &did_nothing_left);

  BIND(&did_something_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             did_something);
  }

  BIND(&did_nothing_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             &didnt_do_anything);
  }

  BIND(&didnt_do_anything);
  // Fall through if neither string was an indirect string.
}

TNode<String> StringBuiltinsAssembler::DerefIndirectString(
    TNode<String> string, TNode<Int32T> instance_type, Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(string, instance_type, &deref, cannot_deref);
  BIND(&deref);
  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  return LoadObjectField<String>(string, ThinString::kActualOffset);
}

TF_BUILTIN(StringAdd_CheckNone, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  Node* context = Parameter(Descriptor::kContext);
  Return(StringAdd(context, left, right));
}

TF_BUILTIN(SubString, StringBuiltinsAssembler) {
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Smi> from = CAST(Parameter(Descriptor::kFrom));
  TNode<Smi> to = CAST(Parameter(Descriptor::kTo));
  Return(SubString(string, SmiUntag(from), SmiUntag(to)));
}

void StringBuiltinsAssembler::GenerateStringRelationalComparison(
    TNode<String> left, TNode<String> right, Operation op) {
  TVARIABLE(String, var_left, left);
  TVARIABLE(String, var_right, right);

  Label if_less(this), if_equal(this), if_greater(this);
  Label restart(this, {&var_left, &var_right});
  Goto(&restart);
  BIND(&restart);

  TNode<String> lhs = var_left.value();
  TNode<String> rhs = var_right.value();
  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  GotoIf(TaggedEqual(lhs, rhs), &if_equal);

  // Load instance types of {lhs} and {rhs}.
  TNode<Uint16T> lhs_instance_type = LoadInstanceType(lhs);
  TNode<Uint16T> rhs_instance_type = LoadInstanceType(rhs);

  // Combine the instance types into a single 16-bit value, so we can check
  // both of them at once.
  TNode<Int32T> both_instance_types = Word32Or(
      lhs_instance_type, Word32Shl(rhs_instance_type, Int32Constant(8)));

  // Check that both {lhs} and {rhs} are flat one-byte strings.
  int const kBothSeqOneByteStringMask =
      kStringEncodingMask | kStringRepresentationMask |
      ((kStringEncodingMask | kStringRepresentationMask) << 8);
  int const kBothSeqOneByteStringTag =
      kOneByteStringTag | kSeqStringTag |
      ((kOneByteStringTag | kSeqStringTag) << 8);
  Label if_bothonebyteseqstrings(this), if_notbothonebyteseqstrings(this);
  Branch(Word32Equal(Word32And(both_instance_types,
                               Int32Constant(kBothSeqOneByteStringMask)),
                     Int32Constant(kBothSeqOneByteStringTag)),
         &if_bothonebyteseqstrings, &if_notbothonebyteseqstrings);

  BIND(&if_bothonebyteseqstrings);
  {
    // Load the length of {lhs} and {rhs}.
    TNode<IntPtrT> lhs_length = LoadStringLengthAsWord(lhs);
    TNode<IntPtrT> rhs_length = LoadStringLengthAsWord(rhs);

    // Determine the minimum length.
    TNode<IntPtrT> length = IntPtrMin(lhs_length, rhs_length);

    // Compute the effective offset of the first character.
    TNode<IntPtrT> begin =
        IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag);

    // Compute the first offset after the string from the length.
    TNode<IntPtrT> end = IntPtrAdd(begin, length);

    // Loop over the {lhs} and {rhs} strings to see if they are equal.
    TVARIABLE(IntPtrT, var_offset, begin);
    Label loop(this, &var_offset);
    Goto(&loop);
    BIND(&loop);
    {
      // Check if {offset} equals {end}.
      Label if_done(this), if_notdone(this);
      Branch(WordEqual(var_offset.value(), end), &if_done, &if_notdone);

      BIND(&if_notdone);
      {
        // Load the next characters from {lhs} and {rhs}.
        TNode<Uint8T> lhs_value = Load<Uint8T>(lhs, var_offset.value());
        TNode<Uint8T> rhs_value = Load<Uint8T>(rhs, var_offset.value());

        // Check if the characters match.
        Label if_valueissame(this), if_valueisnotsame(this);
        Branch(Word32Equal(lhs_value, rhs_value), &if_valueissame,
               &if_valueisnotsame);

        BIND(&if_valueissame);
        {
          // Advance to next character.
          var_offset = IntPtrAdd(var_offset.value(), IntPtrConstant(1));
        }
        Goto(&loop);

        BIND(&if_valueisnotsame);
        Branch(Uint32LessThan(lhs_value, rhs_value), &if_less, &if_greater);
      }

      BIND(&if_done);
      {
        // All characters up to the min length are equal, decide based on
        // string length.
        GotoIf(IntPtrEqual(lhs_length, rhs_length), &if_equal);
        Branch(IntPtrLessThan(lhs_length, rhs_length), &if_less, &if_greater);
      }
    }
  }

  BIND(&if_notbothonebyteseqstrings);
  {
    // Try to unwrap indirect strings, restart the above attempt on success.
    MaybeDerefIndirectStrings(&var_left, lhs_instance_type, &var_right,
                              rhs_instance_type, &restart);
    // TODO(bmeurer): Add support for two byte string relational comparisons.
    switch (op) {
      case Operation::kLessThan:
        TailCallRuntime(Runtime::kStringLessThan, NoContextConstant(), lhs,
                        rhs);
        break;
      case Operation::kLessThanOrEqual:
        TailCallRuntime(Runtime::kStringLessThanOrEqual, NoContextConstant(),
                        lhs, rhs);
        break;
      case Operation::kGreaterThan:
        TailCallRuntime(Runtime::kStringGreaterThan, NoContextConstant(), lhs,
                        rhs);
        break;
      case Operation::kGreaterThanOrEqual:
        TailCallRuntime(Runtime::kStringGreaterThanOrEqual, NoContextConstant(),
                        lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
  }

  BIND(&if_less);
  switch (op) {
    case Operation::kLessThan:
    case Operation::kLessThanOrEqual:
      Return(TrueConstant());
      break;

    case Operation::kGreaterThan:
    case Operation::kGreaterThanOrEqual:
      Return(FalseConstant());
      break;
    default:
      UNREACHABLE();
  }

  BIND(&if_equal);
  switch (op) {
    case Operation::kLessThan:
    case Operation::kGreaterThan:
      Return(FalseConstant());
      break;

    case Operation::kLessThanOrEqual:
    case Operation::kGreaterThanOrEqual:
      Return(TrueConstant());
      break;
    default:
      UNREACHABLE();
  }

  BIND(&if_greater);
  switch (op) {
    case Operation::kLessThan:
    case Operation::kLessThanOrEqual:
      Return(FalseConstant());
      break;

    case Operation::kGreaterThan:
    case Operation::kGreaterThanOrEqual:
      Return(TrueConstant());
      break;
    default:
      UNREACHABLE();
  }
}

TF_BUILTIN(StringEqual, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  GenerateStringEqual(left, right);
}

TF_BUILTIN(StringLessThan, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  GenerateStringRelationalComparison(left, right, Operation::kLessThan);
}

TF_BUILTIN(StringLessThanOrEqual, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  GenerateStringRelationalComparison(left, right, Operation::kLessThanOrEqual);
}

TF_BUILTIN(StringGreaterThan, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  GenerateStringRelationalComparison(left, right, Operation::kGreaterThan);
}

TF_BUILTIN(StringGreaterThanOrEqual, StringBuiltinsAssembler) {
  TNode<String> left = CAST(Parameter(Descriptor::kLeft));
  TNode<String> right = CAST(Parameter(Descriptor::kRight));
  GenerateStringRelationalComparison(left, right,
                                     Operation::kGreaterThanOrEqual);
}

TF_BUILTIN(StringCodePointAt, StringBuiltinsAssembler) {
  TNode<String> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<IntPtrT> position =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kPosition));

  // TODO(sigurds) Figure out if passing length as argument pays off.
  TNode<IntPtrT> length = LoadStringLengthAsWord(receiver);
  // Load the character code at the {position} from the {receiver}.
  TNode<Int32T> code =
      LoadSurrogatePairAt(receiver, length, position, UnicodeEncoding::UTF32);
  // And return it as TaggedSigned value.
  // TODO(turbofan): Allow builtins to return values untagged.
  TNode<Smi> result = SmiFromInt32(code);
  Return(result);
}

TF_BUILTIN(StringFromCodePointAt, StringBuiltinsAssembler) {
  TNode<String> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<IntPtrT> position =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kPosition));

  // TODO(sigurds) Figure out if passing length as argument pays off.
  TNode<IntPtrT> length = LoadStringLengthAsWord(receiver);
  // Load the character code at the {position} from the {receiver}.
  TNode<Int32T> code =
      LoadSurrogatePairAt(receiver, length, position, UnicodeEncoding::UTF16);
  // Create a String from the UTF16 encoded code point
  TNode<String> result = StringFromSingleUTF16EncodedCodePoint(code);
  Return(result);
}

// -----------------------------------------------------------------------------
// ES6 section 21.1 String Objects

// ES6 #sec-string.fromcharcode
TF_BUILTIN(StringFromCharCode, StringBuiltinsAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CodeStubArguments arguments(this, argc);
  // Check if we have exactly one argument (plus the implicit receiver), i.e.
  // if the parent frame is not an arguments adaptor frame.
  Label if_oneargument(this), if_notoneargument(this);
  Branch(Word32Equal(argc, Int32Constant(1)), &if_oneargument,
         &if_notoneargument);

  BIND(&if_oneargument);
  {
    // Single argument case, perform fast single character string cache lookup
    // for one-byte code units, or fall back to creating a single character
    // string on the fly otherwise.
    TNode<Object> code = arguments.AtIndex(0);
    TNode<Word32T> code32 = TruncateTaggedToWord32(context, code);
    TNode<Int32T> code16 =
        Signed(Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit)));
    TNode<String> result = StringFromSingleCharCode(code16);
    arguments.PopAndReturn(result);
  }

  TNode<Word32T> code16;
  BIND(&if_notoneargument);
  {
    Label two_byte(this);
    // Assume that the resulting string contains only one-byte characters.
    TNode<String> one_byte_result = AllocateSeqOneByteString(Unsigned(argc));

    TVARIABLE(IntPtrT, var_max_index, IntPtrConstant(0));

    // Iterate over the incoming arguments, converting them to 8-bit character
    // codes. Stop if any of the conversions generates a code that doesn't fit
    // in 8 bits.
    CodeStubAssembler::VariableList vars({&var_max_index}, zone());
    arguments.ForEach(vars, [&](TNode<Object> arg) {
      TNode<Word32T> code32 = TruncateTaggedToWord32(context, arg);
      code16 = Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

      GotoIf(
          Int32GreaterThan(code16, Int32Constant(String::kMaxOneByteCharCode)),
          &two_byte);

      // The {code16} fits into the SeqOneByteString {one_byte_result}.
      TNode<IntPtrT> offset = ElementOffsetFromIndex(
          var_max_index.value(), UINT8_ELEMENTS,
          SeqOneByteString::kHeaderSize - kHeapObjectTag);
      StoreNoWriteBarrier(MachineRepresentation::kWord8, one_byte_result,
                          offset, code16);
      var_max_index = IntPtrAdd(var_max_index.value(), IntPtrConstant(1));
    });
    arguments.PopAndReturn(one_byte_result);

    BIND(&two_byte);

    // At least one of the characters in the string requires a 16-bit
    // representation.  Allocate a SeqTwoByteString to hold the resulting
    // string.
    TNode<String> two_byte_result = AllocateSeqTwoByteString(Unsigned(argc));

    // Copy the characters that have already been put in the 8-bit string into
    // their corresponding positions in the new 16-bit string.
    TNode<IntPtrT> zero = IntPtrConstant(0);
    CopyStringCharacters(one_byte_result, two_byte_result, zero, zero,
                         var_max_index.value(), String::ONE_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING);

    // Write the character that caused the 8-bit to 16-bit fault.
    TNode<IntPtrT> max_index_offset =
        ElementOffsetFromIndex(var_max_index.value(), UINT16_ELEMENTS,
                               SeqTwoByteString::kHeaderSize - kHeapObjectTag);
    StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                        max_index_offset, code16);
    var_max_index = IntPtrAdd(var_max_index.value(), IntPtrConstant(1));

    // Resume copying the passed-in arguments from the same place where the
    // 8-bit copy stopped, but this time copying over all of the characters
    // using a 16-bit representation.
    arguments.ForEach(
        vars,
        [&](TNode<Object> arg) {
          TNode<Word32T> code32 = TruncateTaggedToWord32(context, arg);
          TNode<Word32T> code16 =
              Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

          TNode<IntPtrT> offset = ElementOffsetFromIndex(
              var_max_index.value(), UINT16_ELEMENTS,
              SeqTwoByteString::kHeaderSize - kHeapObjectTag);
          StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                              offset, code16);
          var_max_index = IntPtrAdd(var_max_index.value(), IntPtrConstant(1));
        },
        var_max_index.value());

    arguments.PopAndReturn(two_byte_result);
  }
}

void StringBuiltinsAssembler::StringIndexOf(
    const TNode<String> subject_string, const TNode<String> search_string,
    const TNode<Smi> position,
    const std::function<void(TNode<Smi>)>& f_return) {
  const TNode<IntPtrT> int_zero = IntPtrConstant(0);
  const TNode<IntPtrT> search_length = LoadStringLengthAsWord(search_string);
  const TNode<IntPtrT> subject_length = LoadStringLengthAsWord(subject_string);
  const TNode<IntPtrT> start_position = IntPtrMax(SmiUntag(position), int_zero);

  Label zero_length_needle(this), return_minus_1(this);
  {
    GotoIf(IntPtrEqual(int_zero, search_length), &zero_length_needle);

    // Check that the needle fits in the start position.
    GotoIfNot(IntPtrLessThanOrEqual(search_length,
                                    IntPtrSub(subject_length, start_position)),
              &return_minus_1);
  }

  // If the string pointers are identical, we can just return 0. Note that this
  // implies {start_position} == 0 since we've passed the check above.
  Label return_zero(this);
  GotoIf(TaggedEqual(subject_string, search_string), &return_zero);

  // Try to unpack subject and search strings. Bail to runtime if either needs
  // to be flattened.
  ToDirectStringAssembler subject_to_direct(state(), subject_string);
  ToDirectStringAssembler search_to_direct(state(), search_string);

  Label call_runtime_unchecked(this, Label::kDeferred);

  subject_to_direct.TryToDirect(&call_runtime_unchecked);
  search_to_direct.TryToDirect(&call_runtime_unchecked);

  // Load pointers to string data.
  const TNode<RawPtrT> subject_ptr =
      subject_to_direct.PointerToData(&call_runtime_unchecked);
  const TNode<RawPtrT> search_ptr =
      search_to_direct.PointerToData(&call_runtime_unchecked);

  const TNode<IntPtrT> subject_offset = subject_to_direct.offset();
  const TNode<IntPtrT> search_offset = search_to_direct.offset();

  // Like String::IndexOf, the actual matching is done by the optimized
  // SearchString method in string-search.h. Dispatch based on string instance
  // types, then call straight into C++ for matching.

  CSA_ASSERT(this, IntPtrGreaterThan(search_length, int_zero));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(start_position, int_zero));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(subject_length, start_position));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(search_length,
                                   IntPtrSub(subject_length, start_position)));

  Label one_one(this), one_two(this), two_one(this), two_two(this);
  DispatchOnStringEncodings(subject_to_direct.instance_type(),
                            search_to_direct.instance_type(), &one_one,
                            &one_two, &two_one, &two_two);

  using onebyte_t = const uint8_t;
  using twobyte_t = const uc16;

  BIND(&one_one);
  {
    const TNode<RawPtrT> adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    const TNode<RawPtrT> adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    Label direct_memchr_call(this), generic_fast_path(this);
    Branch(IntPtrEqual(search_length, IntPtrConstant(1)), &direct_memchr_call,
           &generic_fast_path);

    // An additional fast path that calls directly into memchr for 1-length
    // search strings.
    BIND(&direct_memchr_call);
    {
      const TNode<RawPtrT> string_addr =
          RawPtrAdd(adjusted_subject_ptr, start_position);
      const TNode<IntPtrT> search_length =
          IntPtrSub(subject_length, start_position);
      const TNode<IntPtrT> search_byte =
          ChangeInt32ToIntPtr(Load(MachineType::Uint8(), adjusted_search_ptr));

      const TNode<ExternalReference> memchr =
          ExternalConstant(ExternalReference::libc_memchr_function());
      const TNode<RawPtrT> result_address = UncheckedCast<RawPtrT>(
          CallCFunction(memchr, MachineType::Pointer(),
                        std::make_pair(MachineType::Pointer(), string_addr),
                        std::make_pair(MachineType::IntPtr(), search_byte),
                        std::make_pair(MachineType::UintPtr(), search_length)));
      GotoIf(WordEqual(result_address, int_zero), &return_minus_1);
      const TNode<IntPtrT> result_index =
          IntPtrAdd(RawPtrSub(result_address, string_addr), start_position);
      f_return(SmiTag(result_index));
    }

    BIND(&generic_fast_path);
    {
      const TNode<IntPtrT> result = CallSearchStringRaw<onebyte_t, onebyte_t>(
          adjusted_subject_ptr, subject_length, adjusted_search_ptr,
          search_length, start_position);
      f_return(SmiTag(result));
    }
  }

  BIND(&one_two);
  {
    const TNode<RawPtrT> adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    const TNode<RawPtrT> adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::TWO_BYTE_ENCODING);

    const TNode<IntPtrT> result = CallSearchStringRaw<onebyte_t, twobyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_one);
  {
    const TNode<RawPtrT> adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    const TNode<RawPtrT> adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    const TNode<IntPtrT> result = CallSearchStringRaw<twobyte_t, onebyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_two);
  {
    const TNode<RawPtrT> adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    const TNode<RawPtrT> adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::TWO_BYTE_ENCODING);

    const TNode<IntPtrT> result = CallSearchStringRaw<twobyte_t, twobyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&return_minus_1);
  f_return(SmiConstant(-1));

  BIND(&return_zero);
  f_return(SmiConstant(0));

  BIND(&zero_length_needle);
  {
    Comment("0-length search_string");
    f_return(SmiTag(IntPtrMin(subject_length, start_position)));
  }

  BIND(&call_runtime_unchecked);
  {
    // Simplified version of the runtime call where the types of the arguments
    // are already known due to type checks in this stub.
    Comment("Call Runtime Unchecked");
    TNode<Smi> result =
        CAST(CallRuntime(Runtime::kStringIndexOfUnchecked, NoContextConstant(),
                         subject_string, search_string, position));
    f_return(result);
  }
}

// ES6 String.prototype.indexOf(searchString [, position])
// #sec-string.prototype.indexof
// Unchecked helper for builtins lowering.
TF_BUILTIN(StringIndexOf, StringBuiltinsAssembler) {
  TNode<String> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> search_string = CAST(Parameter(Descriptor::kSearchString));
  TNode<Smi> position = CAST(Parameter(Descriptor::kPosition));
  StringIndexOf(receiver, search_string, position,
                [this](TNode<Smi> result) { this->Return(result); });
}

// ES6 String.prototype.includes(searchString [, position])
// #sec-string.prototype.includes
TF_BUILTIN(StringPrototypeIncludes, StringIncludesIndexOfAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Generate(kIncludes, argc, context);
}

// ES6 String.prototype.indexOf(searchString [, position])
// #sec-string.prototype.indexof
TF_BUILTIN(StringPrototypeIndexOf, StringIncludesIndexOfAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Generate(kIndexOf, argc, context);
}

void StringIncludesIndexOfAssembler::Generate(SearchVariant variant,
                                              TNode<IntPtrT> argc,
                                              TNode<Context> context) {
  CodeStubArguments arguments(this, argc);
  const TNode<Object> receiver = arguments.GetReceiver();

  TVARIABLE(Object, var_search_string);
  TVARIABLE(Object, var_position);
  Label argc_1(this), argc_2(this), call_runtime(this, Label::kDeferred),
      fast_path(this);

  GotoIf(IntPtrEqual(argc, IntPtrConstant(1)), &argc_1);
  GotoIf(IntPtrGreaterThan(argc, IntPtrConstant(1)), &argc_2);
  {
    Comment("0 Argument case");
    CSA_ASSERT(this, IntPtrEqual(argc, IntPtrConstant(0)));
    TNode<Oddball> undefined = UndefinedConstant();
    var_search_string = undefined;
    var_position = undefined;
    Goto(&call_runtime);
  }
  BIND(&argc_1);
  {
    Comment("1 Argument case");
    var_search_string = arguments.AtIndex(0);
    var_position = SmiConstant(0);
    Goto(&fast_path);
  }
  BIND(&argc_2);
  {
    Comment("2 Argument case");
    var_search_string = arguments.AtIndex(0);
    var_position = arguments.AtIndex(1);
    GotoIfNot(TaggedIsSmi(var_position.value()), &call_runtime);
    Goto(&fast_path);
  }
  BIND(&fast_path);
  {
    Comment("Fast Path");
    const TNode<Object> search = var_search_string.value();
    const TNode<Smi> position = CAST(var_position.value());
    GotoIf(TaggedIsSmi(receiver), &call_runtime);
    GotoIf(TaggedIsSmi(search), &call_runtime);
    GotoIfNot(IsString(CAST(receiver)), &call_runtime);
    GotoIfNot(IsString(CAST(search)), &call_runtime);

    StringIndexOf(CAST(receiver), CAST(search), position,
                  [&](TNode<Smi> result) {
                    if (variant == kIndexOf) {
                      arguments.PopAndReturn(result);
                    } else {
                      arguments.PopAndReturn(SelectBooleanConstant(
                          SmiGreaterThanOrEqual(result, SmiConstant(0))));
                    }
                  });
  }
  BIND(&call_runtime);
  {
    Comment("Call Runtime");
    Runtime::FunctionId runtime = variant == kIndexOf
                                      ? Runtime::kStringIndexOf
                                      : Runtime::kStringIncludes;
    const TNode<Object> result =
        CallRuntime(runtime, context, receiver, var_search_string.value(),
                    var_position.value());
    arguments.PopAndReturn(result);
  }
}

void StringBuiltinsAssembler::MaybeCallFunctionAtSymbol(
    const TNode<Context> context, const TNode<Object> object,
    const TNode<Object> maybe_string, Handle<Symbol> symbol,
    DescriptorIndexNameValue additional_property_to_check,
    const NodeFunction0& regexp_call, const NodeFunction1& generic_call) {
  Label out(this);

  // Smis definitely don't have an attached symbol.
  GotoIf(TaggedIsSmi(object), &out);
  TNode<HeapObject> heap_object = CAST(object);

  // Take the fast path for RegExps.
  // There's two conditions: {object} needs to be a fast regexp, and
  // {maybe_string} must be a string (we can't call ToString on the fast path
  // since it may mutate {object}).
  {
    Label stub_call(this), slow_lookup(this);

    GotoIf(TaggedIsSmi(maybe_string), &slow_lookup);
    GotoIfNot(IsString(CAST(maybe_string)), &slow_lookup);

    // Note we don't run a full (= permissive) check here, because passing the
    // check implies calling the fast variants of target builtins, which assume
    // we've already made their appropriate fast path checks. This is not the
    // case though; e.g.: some of the target builtins access flag getters.
    // TODO(jgruber): Handle slow flag accesses on the fast path and make this
    // permissive.
    RegExpBuiltinsAssembler regexp_asm(state());
    regexp_asm.BranchIfFastRegExp(
        context, heap_object, LoadMap(heap_object),
        PrototypeCheckAssembler::kCheckPrototypePropertyConstness,
        additional_property_to_check, &stub_call, &slow_lookup);

    BIND(&stub_call);
    // TODO(jgruber): Add a no-JS scope once it exists.
    regexp_call();

    BIND(&slow_lookup);
  }

  GotoIf(IsNullOrUndefined(heap_object), &out);

  // Fall back to a slow lookup of {heap_object[symbol]}.
  //
  // The spec uses GetMethod({heap_object}, {symbol}), which has a few quirks:
  // * null values are turned into undefined, and
  // * an exception is thrown if the value is not undefined, null, or callable.
  // We handle the former by jumping to {out} for null values as well, while
  // the latter is already handled by the Call({maybe_func}) operation.

  const TNode<Object> maybe_func = GetProperty(context, heap_object, symbol);
  GotoIf(IsUndefined(maybe_func), &out);
  GotoIf(IsNull(maybe_func), &out);

  // Attempt to call the function.
  generic_call(maybe_func);

  BIND(&out);
}

const TNode<Smi> StringBuiltinsAssembler::IndexOfDollarChar(
    const TNode<Context> context, const TNode<String> string) {
  const TNode<String> dollar_string = HeapConstant(
      isolate()->factory()->LookupSingleCharacterStringFromCode('$'));
  const TNode<Smi> dollar_ix =
      CAST(CallBuiltin(Builtins::kStringIndexOf, context, string, dollar_string,
                       SmiConstant(0)));
  return dollar_ix;
}

TNode<String> StringBuiltinsAssembler::GetSubstitution(
    TNode<Context> context, TNode<String> subject_string,
    TNode<Smi> match_start_index, TNode<Smi> match_end_index,
    TNode<String> replace_string) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(match_start_index));
  CSA_ASSERT(this, TaggedIsPositiveSmi(match_end_index));

  TVARIABLE(String, var_result, replace_string);
  Label runtime(this), out(this);

  // In this primitive implementation we simply look for the next '$' char in
  // {replace_string}. If it doesn't exist, we can simply return
  // {replace_string} itself. If it does, then we delegate to
  // String::GetSubstitution, passing in the index of the first '$' to avoid
  // repeated scanning work.
  // TODO(jgruber): Possibly extend this in the future to handle more complex
  // cases without runtime calls.

  const TNode<Smi> dollar_index = IndexOfDollarChar(context, replace_string);
  Branch(SmiIsNegative(dollar_index), &out, &runtime);

  BIND(&runtime);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(dollar_index));

    const TNode<Object> matched =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_start_index), SmiUntag(match_end_index));
    const TNode<String> replacement_string = CAST(
        CallRuntime(Runtime::kGetSubstitution, context, matched, subject_string,
                    match_start_index, replace_string, dollar_index));
    var_result = replacement_string;

    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES6 #sec-string.prototype.replace
TF_BUILTIN(StringPrototypeReplace, StringBuiltinsAssembler) {
  Label out(this);

  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  const TNode<Object> search = CAST(Parameter(Descriptor::kSearch));
  const TNode<Object> replace = CAST(Parameter(Descriptor::kReplace));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  const TNode<Smi> smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.replace");

  // Redirect to replacer method if {search[@@replace]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, search, receiver, isolate()->factory()->replace_symbol(),
      DescriptorIndexNameValue{JSRegExp::kSymbolReplaceFunctionDescriptorIndex,
                               RootIndex::kreplace_symbol,
                               Context::REGEXP_REPLACE_FUNCTION_INDEX},
      [=]() {
        Return(CallBuiltin(Builtins::kRegExpReplace, context, search, receiver,
                           replace));
      },
      [=](TNode<Object> fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        Return(CallJS(call_callable, context, fn, search, receiver, replace));
      });

  // Convert {receiver} and {search} to strings.

  const TNode<String> subject_string = ToString_Inline(context, receiver);
  const TNode<String> search_string = ToString_Inline(context, search);

  const TNode<IntPtrT> subject_length = LoadStringLengthAsWord(subject_string);
  const TNode<IntPtrT> search_length = LoadStringLengthAsWord(search_string);

  // Fast-path single-char {search}, long cons {receiver}, and simple string
  // {replace}.
  {
    Label next(this);

    GotoIfNot(WordEqual(search_length, IntPtrConstant(1)), &next);
    GotoIfNot(IntPtrGreaterThan(subject_length, IntPtrConstant(0xFF)), &next);
    GotoIf(TaggedIsSmi(replace), &next);
    GotoIfNot(IsString(CAST(replace)), &next);

    TNode<String> replace_string = CAST(replace);
    const TNode<Uint16T> subject_instance_type =
        LoadInstanceType(subject_string);
    GotoIfNot(IsConsStringInstanceType(subject_instance_type), &next);

    GotoIf(TaggedIsPositiveSmi(IndexOfDollarChar(context, replace_string)),
           &next);

    // Searching by traversing a cons string tree and replace with cons of
    // slices works only when the replaced string is a single character, being
    // replaced by a simple string and only pays off for long strings.
    // TODO(jgruber): Reevaluate if this is still beneficial.
    // TODO(jgruber): TailCallRuntime when it correctly handles adapter frames.
    Return(CallRuntime(Runtime::kStringReplaceOneCharWithString, context,
                       subject_string, search_string, replace_string));

    BIND(&next);
  }

  // TODO(jgruber): Extend StringIndexOf to handle two-byte strings and
  // longer substrings - we can handle up to 8 chars (one-byte) / 4 chars
  // (2-byte).

  const TNode<Smi> match_start_index =
      CAST(CallBuiltin(Builtins::kStringIndexOf, context, subject_string,
                       search_string, smi_zero));

  // Early exit if no match found.
  {
    Label next(this), return_subject(this);

    GotoIfNot(SmiIsNegative(match_start_index), &next);

    // The spec requires to perform ToString(replace) if the {replace} is not
    // callable even if we are going to exit here.
    // Since ToString() being applied to Smi does not have side effects for
    // numbers we can skip it.
    GotoIf(TaggedIsSmi(replace), &return_subject);
    GotoIf(IsCallableMap(LoadMap(CAST(replace))), &return_subject);

    // TODO(jgruber): Could introduce ToStringSideeffectsStub which only
    // performs observable parts of ToString.
    ToString_Inline(context, replace);
    Goto(&return_subject);

    BIND(&return_subject);
    Return(subject_string);

    BIND(&next);
  }

  const TNode<Smi> match_end_index =
      SmiAdd(match_start_index, SmiFromIntPtr(search_length));

  TVARIABLE(String, var_result, EmptyStringConstant());

  // Compute the prefix.
  {
    Label next(this);

    GotoIf(SmiEqual(match_start_index, smi_zero), &next);
    const TNode<String> prefix =
        CAST(CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                         IntPtrConstant(0), SmiUntag(match_start_index)));
    var_result = prefix;

    Goto(&next);
    BIND(&next);
  }

  // Compute the string to replace with.

  Label if_iscallablereplace(this), if_notcallablereplace(this);
  GotoIf(TaggedIsSmi(replace), &if_notcallablereplace);
  Branch(IsCallableMap(LoadMap(CAST(replace))), &if_iscallablereplace,
         &if_notcallablereplace);

  BIND(&if_iscallablereplace);
  {
    Callable call_callable = CodeFactory::Call(isolate());
    const TNode<Object> replacement =
        CallJS(call_callable, context, replace, UndefinedConstant(),
               search_string, match_start_index, subject_string);
    const TNode<String> replacement_string =
        ToString_Inline(context, replacement);
    var_result = CAST(CallBuiltin(Builtins::kStringAdd_CheckNone, context,
                                  var_result.value(), replacement_string));
    Goto(&out);
  }

  BIND(&if_notcallablereplace);
  {
    const TNode<String> replace_string = ToString_Inline(context, replace);
    const TNode<Object> replacement =
        GetSubstitution(context, subject_string, match_start_index,
                        match_end_index, replace_string);
    var_result = CAST(CallBuiltin(Builtins::kStringAdd_CheckNone, context,
                                  var_result.value(), replacement));
    Goto(&out);
  }

  BIND(&out);
  {
    const TNode<Object> suffix =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_end_index), subject_length);
    const TNode<Object> result = CallBuiltin(
        Builtins::kStringAdd_CheckNone, context, var_result.value(), suffix);
    Return(result);
  }
}

class StringMatchSearchAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringMatchSearchAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

 protected:
  enum Variant { kMatch, kSearch };

  void Generate(Variant variant, const char* method_name,
                TNode<Object> receiver, TNode<Object> maybe_regexp,
                TNode<Context> context) {
    Label call_regexp_match_search(this);

    Builtins::Name builtin;
    Handle<Symbol> symbol;
    DescriptorIndexNameValue property_to_check;
    if (variant == kMatch) {
      builtin = Builtins::kRegExpMatchFast;
      symbol = isolate()->factory()->match_symbol();
      property_to_check = DescriptorIndexNameValue{
          JSRegExp::kSymbolMatchFunctionDescriptorIndex,
          RootIndex::kmatch_symbol, Context::REGEXP_MATCH_FUNCTION_INDEX};
    } else {
      builtin = Builtins::kRegExpSearchFast;
      symbol = isolate()->factory()->search_symbol();
      property_to_check = DescriptorIndexNameValue{
          JSRegExp::kSymbolSearchFunctionDescriptorIndex,
          RootIndex::ksearch_symbol, Context::REGEXP_SEARCH_FUNCTION_INDEX};
    }

    RequireObjectCoercible(context, receiver, method_name);

    MaybeCallFunctionAtSymbol(
        context, maybe_regexp, receiver, symbol, property_to_check,
        [=] { Return(CallBuiltin(builtin, context, maybe_regexp, receiver)); },
        [=](TNode<Object> fn) {
          Callable call_callable = CodeFactory::Call(isolate());
          Return(CallJS(call_callable, context, fn, maybe_regexp, receiver));
        });

    // maybe_regexp is not a RegExp nor has [@@match / @@search] property.
    {
      RegExpBuiltinsAssembler regexp_asm(state());

      TNode<String> receiver_string = ToString_Inline(context, receiver);
      TNode<NativeContext> native_context = LoadNativeContext(context);
      TNode<HeapObject> regexp_function = CAST(
          LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX));
      TNode<Map> initial_map = CAST(LoadObjectField(
          regexp_function, JSFunction::kPrototypeOrInitialMapOffset));
      TNode<Object> regexp = regexp_asm.RegExpCreate(
          context, initial_map, maybe_regexp, EmptyStringConstant());

      // TODO(jgruber): Handle slow flag accesses on the fast path and make this
      // permissive.
      Label fast_path(this), slow_path(this);
      regexp_asm.BranchIfFastRegExp(
          context, CAST(regexp), initial_map,
          PrototypeCheckAssembler::kCheckPrototypePropertyConstness,
          property_to_check, &fast_path, &slow_path);

      BIND(&fast_path);
      Return(CallBuiltin(builtin, context, regexp, receiver_string));

      BIND(&slow_path);
      {
        TNode<Object> maybe_func = GetProperty(context, regexp, symbol);
        Callable call_callable = CodeFactory::Call(isolate());
        Return(CallJS(call_callable, context, maybe_func, regexp,
                      receiver_string));
      }
    }
  }
};

// ES6 #sec-string.prototype.match
TF_BUILTIN(StringPrototypeMatch, StringMatchSearchAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_regexp = CAST(Parameter(Descriptor::kRegexp));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(kMatch, "String.prototype.match", receiver, maybe_regexp, context);
}

// ES #sec-string.prototype.matchAll
TF_BUILTIN(StringPrototypeMatchAll, StringBuiltinsAssembler) {
  char const* method_name = "String.prototype.matchAll";

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> maybe_regexp = CAST(Parameter(Descriptor::kRegexp));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<NativeContext> native_context = LoadNativeContext(context);

  // 1. Let O be ? RequireObjectCoercible(this value).
  RequireObjectCoercible(context, receiver, method_name);

  RegExpMatchAllAssembler regexp_asm(state());
  {
    Label fast(this), slow(this, Label::kDeferred),
        throw_exception(this, Label::kDeferred),
        throw_flags_exception(this, Label::kDeferred), next(this);

    // 2. If regexp is neither undefined nor null, then
    //   a. Let isRegExp be ? IsRegExp(regexp).
    //   b. If isRegExp is true, then
    //     i. Let flags be ? Get(regexp, "flags").
    //    ii. Perform ? RequireObjectCoercible(flags).
    //   iii. If ? ToString(flags) does not contain "g", throw a
    //        TypeError exception.
    GotoIf(TaggedIsSmi(maybe_regexp), &next);
    TNode<HeapObject> heap_maybe_regexp = CAST(maybe_regexp);
    regexp_asm.BranchIfFastRegExp_Strict(context, heap_maybe_regexp, &fast,
                                         &slow);

    BIND(&fast);
    {
      TNode<BoolT> is_global = regexp_asm.FlagGetter(context, heap_maybe_regexp,
                                                     JSRegExp::kGlobal, true);
      Branch(is_global, &next, &throw_exception);
    }

    BIND(&slow);
    {
      GotoIfNot(regexp_asm.IsRegExp(native_context, heap_maybe_regexp), &next);

      TNode<Object> flags = GetProperty(context, heap_maybe_regexp,
                                        isolate()->factory()->flags_string());
      // TODO(syg): Implement a RequireObjectCoercible with more flexible error
      // messages.
      GotoIf(IsNullOrUndefined(flags), &throw_flags_exception);

      TNode<String> flags_string = ToString_Inline(context, flags);
      TNode<String> global_char_string = StringConstant("g");
      TNode<Smi> global_ix =
          CAST(CallBuiltin(Builtins::kStringIndexOf, context, flags_string,
                           global_char_string, SmiConstant(0)));
      Branch(SmiEqual(global_ix, SmiConstant(-1)), &throw_exception, &next);
    }

    BIND(&throw_exception);
    ThrowTypeError(context, MessageTemplate::kRegExpGlobalInvokedOnNonGlobal);

    BIND(&throw_flags_exception);
    ThrowTypeError(context,
                   MessageTemplate::kStringMatchAllNullOrUndefinedFlags);

    BIND(&next);
  }
  //   a. Let matcher be ? GetMethod(regexp, @@matchAll).
  //   b. If matcher is not undefined, then
  //     i. Return ? Call(matcher, regexp, « O »).
  auto if_regexp_call = [&] {
    // MaybeCallFunctionAtSymbol guarantees fast path is chosen only if
    // maybe_regexp is a fast regexp and receiver is a string.
    TNode<String> s = CAST(receiver);

    Return(
        RegExpPrototypeMatchAllImpl(context, native_context, maybe_regexp, s));
  };
  auto if_generic_call = [=](TNode<Object> fn) {
    Callable call_callable = CodeFactory::Call(isolate());
    Return(CallJS(call_callable, context, fn, maybe_regexp, receiver));
  };
  MaybeCallFunctionAtSymbol(
      context, maybe_regexp, receiver, isolate()->factory()->match_all_symbol(),
      DescriptorIndexNameValue{JSRegExp::kSymbolMatchAllFunctionDescriptorIndex,
                               RootIndex::kmatch_all_symbol,
                               Context::REGEXP_MATCH_ALL_FUNCTION_INDEX},
      if_regexp_call, if_generic_call);

  // 3. Let S be ? ToString(O).
  TNode<String> s = ToString_Inline(context, receiver);

  // 4. Let rx be ? RegExpCreate(R, "g").
  TNode<Object> rx = regexp_asm.RegExpCreate(context, native_context,
                                             maybe_regexp, StringConstant("g"));

  // 5. Return ? Invoke(rx, @@matchAll, « S »).
  Callable callable = CodeFactory::Call(isolate());
  TNode<Object> match_all_func =
      GetProperty(context, rx, isolate()->factory()->match_all_symbol());
  Return(CallJS(callable, context, match_all_func, rx, s));
}

// ES6 #sec-string.prototype.search
TF_BUILTIN(StringPrototypeSearch, StringMatchSearchAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_regexp = CAST(Parameter(Descriptor::kRegexp));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Generate(kSearch, "String.prototype.search", receiver, maybe_regexp, context);
}

TNode<JSArray> StringBuiltinsAssembler::StringToArray(
    TNode<NativeContext> context, TNode<String> subject_string,
    TNode<Smi> subject_length, TNode<Number> limit_number) {
  CSA_ASSERT(this, SmiGreaterThan(subject_length, SmiConstant(0)));

  Label done(this), call_runtime(this, Label::kDeferred),
      fill_thehole_and_call_runtime(this, Label::kDeferred);
  TVARIABLE(JSArray, result_array);

  TNode<Uint16T> instance_type = LoadInstanceType(subject_string);
  GotoIfNot(IsOneByteStringInstanceType(instance_type), &call_runtime);

  // Try to use cached one byte characters.
  {
    TNode<Smi> length_smi =
        Select<Smi>(TaggedIsSmi(limit_number),
                    [=] { return SmiMin(CAST(limit_number), subject_length); },
                    [=] { return subject_length; });
    TNode<IntPtrT> length = SmiToIntPtr(length_smi);

    ToDirectStringAssembler to_direct(state(), subject_string);
    to_direct.TryToDirect(&call_runtime);
    TNode<FixedArray> elements = CAST(AllocateFixedArray(
        PACKED_ELEMENTS, length, AllocationFlag::kAllowLargeObjectAllocation));
    // Don't allocate anything while {string_data} is live!
    TNode<RawPtrT> string_data =
        to_direct.PointerToData(&fill_thehole_and_call_runtime);
    TNode<IntPtrT> string_data_offset = to_direct.offset();
    TNode<FixedArray> cache = SingleCharacterStringCacheConstant();

    BuildFastLoop<IntPtrT>(
        IntPtrConstant(0), length,
        [&](TNode<IntPtrT> index) {
          // TODO(jkummerow): Implement a CSA version of DisallowHeapAllocation
          // and use that to guard ToDirectStringAssembler.PointerToData().
          CSA_ASSERT(this, WordEqual(to_direct.PointerToData(&call_runtime),
                                     string_data));
          TNode<Int32T> char_code =
              UncheckedCast<Int32T>(Load(MachineType::Uint8(), string_data,
                                         IntPtrAdd(index, string_data_offset)));
          TNode<UintPtrT> code_index = ChangeUint32ToWord(char_code);
          TNode<Object> entry = LoadFixedArrayElement(cache, code_index);

          // If we cannot find a char in the cache, fill the hole for the fixed
          // array, and call runtime.
          GotoIf(IsUndefined(entry), &fill_thehole_and_call_runtime);

          StoreFixedArrayElement(elements, index, entry);
        },
        1, IndexAdvanceMode::kPost);

    TNode<Map> array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, context);
    result_array = AllocateJSArray(array_map, elements, length_smi);
    Goto(&done);

    BIND(&fill_thehole_and_call_runtime);
    {
      FillFixedArrayWithValue(PACKED_ELEMENTS, elements, IntPtrConstant(0),
                              length, RootIndex::kTheHoleValue);
      Goto(&call_runtime);
    }
  }

  BIND(&call_runtime);
  {
    result_array = CAST(CallRuntime(Runtime::kStringToArray, context,
                                    subject_string, limit_number));
    Goto(&done);
  }

  BIND(&done);
  return result_array.value();
}

// ES6 section 21.1.3.19 String.prototype.split ( separator, limit )
TF_BUILTIN(StringPrototypeSplit, StringBuiltinsAssembler) {
  const int kSeparatorArg = 0;
  const int kLimitArg = 1;

  const TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  TNode<Object> receiver = args.GetReceiver();
  const TNode<Object> separator = args.GetOptionalArgumentValue(kSeparatorArg);
  const TNode<Object> limit = args.GetOptionalArgumentValue(kLimitArg);
  TNode<NativeContext> context = CAST(Parameter(Descriptor::kContext));

  TNode<Smi> smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.split");

  // Redirect to splitter method if {separator[@@split]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, separator, receiver, isolate()->factory()->split_symbol(),
      DescriptorIndexNameValue{JSRegExp::kSymbolSplitFunctionDescriptorIndex,
                               RootIndex::ksplit_symbol,
                               Context::REGEXP_SPLIT_FUNCTION_INDEX},
      [&]() {
        args.PopAndReturn(CallBuiltin(Builtins::kRegExpSplit, context,
                                      separator, receiver, limit));
      },
      [&](TNode<Object> fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        args.PopAndReturn(
            CallJS(call_callable, context, fn, separator, receiver, limit));
      });

  // String and integer conversions.

  TNode<String> subject_string = ToString_Inline(context, receiver);
  TNode<Number> limit_number = Select<Number>(
      IsUndefined(limit), [=] { return NumberConstant(kMaxUInt32); },
      [=] { return ToUint32(context, limit); });
  const TNode<String> separator_string = ToString_Inline(context, separator);

  Label return_empty_array(this);

  // Shortcut for {limit} == 0.
  GotoIf(TaggedEqual(limit_number, smi_zero), &return_empty_array);

  // ECMA-262 says that if {separator} is undefined, the result should
  // be an array of size 1 containing the entire string.
  {
    Label next(this);
    GotoIfNot(IsUndefined(separator), &next);

    const ElementsKind kind = PACKED_ELEMENTS;
    const TNode<NativeContext> native_context = LoadNativeContext(context);
    TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);

    TNode<Smi> length = SmiConstant(1);
    TNode<IntPtrT> capacity = IntPtrConstant(1);
    TNode<JSArray> result = AllocateJSArray(kind, array_map, capacity, length);

    TNode<FixedArray> fixed_array = CAST(LoadElements(result));
    StoreFixedArrayElement(fixed_array, 0, subject_string);

    args.PopAndReturn(result);

    BIND(&next);
  }

  // If the separator string is empty then return the elements in the subject.
  {
    Label next(this);
    GotoIfNot(SmiEqual(LoadStringLengthAsSmi(separator_string), smi_zero),
              &next);

    TNode<Smi> subject_length = LoadStringLengthAsSmi(subject_string);
    GotoIf(SmiEqual(subject_length, smi_zero), &return_empty_array);

    args.PopAndReturn(
        StringToArray(context, subject_string, subject_length, limit_number));

    BIND(&next);
  }

  const TNode<Object> result =
      CallRuntime(Runtime::kStringSplit, context, subject_string,
                  separator_string, limit_number);
  args.PopAndReturn(result);

  BIND(&return_empty_array);
  {
    const ElementsKind kind = PACKED_ELEMENTS;
    const TNode<NativeContext> native_context = LoadNativeContext(context);
    TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);

    TNode<Smi> length = smi_zero;
    TNode<IntPtrT> capacity = IntPtrConstant(0);
    TNode<JSArray> result = AllocateJSArray(kind, array_map, capacity, length);

    args.PopAndReturn(result);
  }
}

TF_BUILTIN(StringSubstring, StringBuiltinsAssembler) {
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<IntPtrT> from = UncheckedCast<IntPtrT>(Parameter(Descriptor::kFrom));
  TNode<IntPtrT> to = UncheckedCast<IntPtrT>(Parameter(Descriptor::kTo));

  Return(SubString(string, from, to));
}

// ES6 #sec-string.prototype.trim
TF_BUILTIN(StringPrototypeTrim, StringTrimAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(String::kTrim, "String.prototype.trim", argc, context);
}

// https://github.com/tc39/proposal-string-left-right-trim
TF_BUILTIN(StringPrototypeTrimStart, StringTrimAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(String::kTrimStart, "String.prototype.trimLeft", argc, context);
}

// https://github.com/tc39/proposal-string-left-right-trim
TF_BUILTIN(StringPrototypeTrimEnd, StringTrimAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(String::kTrimEnd, "String.prototype.trimRight", argc, context);
}

void StringTrimAssembler::Generate(String::TrimMode mode,
                                   const char* method_name, TNode<IntPtrT> argc,
                                   TNode<Context> context) {
  Label return_emptystring(this), if_runtime(this);

  CodeStubArguments arguments(this, argc);
  TNode<Object> receiver = arguments.GetReceiver();

  // Check that {receiver} is coercible to Object and convert it to a String.
  const TNode<String> string = ToThisString(context, receiver, method_name);
  const TNode<IntPtrT> string_length = LoadStringLengthAsWord(string);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  const TNode<RawPtrT> string_data = to_direct.PointerToData(&if_runtime);
  const TNode<Int32T> instance_type = to_direct.instance_type();
  const TNode<BoolT> is_stringonebyte =
      IsOneByteStringInstanceType(instance_type);
  const TNode<IntPtrT> string_data_offset = to_direct.offset();

  TVARIABLE(IntPtrT, var_start, IntPtrConstant(0));
  TVARIABLE(IntPtrT, var_end, IntPtrSub(string_length, IntPtrConstant(1)));

  if (mode == String::kTrimStart || mode == String::kTrim) {
    ScanForNonWhiteSpaceOrLineTerminator(string_data, string_data_offset,
                                         is_stringonebyte, &var_start,
                                         string_length, 1, &return_emptystring);
  }
  if (mode == String::kTrimEnd || mode == String::kTrim) {
    ScanForNonWhiteSpaceOrLineTerminator(
        string_data, string_data_offset, is_stringonebyte, &var_end,
        IntPtrConstant(-1), -1, &return_emptystring);
  }

  arguments.PopAndReturn(
      SubString(string, var_start.value(),
                IntPtrAdd(var_end.value(), IntPtrConstant(1))));

  BIND(&if_runtime);
  arguments.PopAndReturn(
      CallRuntime(Runtime::kStringTrim, context, string, SmiConstant(mode)));

  BIND(&return_emptystring);
  arguments.PopAndReturn(EmptyStringConstant());
}

void StringTrimAssembler::ScanForNonWhiteSpaceOrLineTerminator(
    const TNode<RawPtrT> string_data, const TNode<IntPtrT> string_data_offset,
    const TNode<BoolT> is_stringonebyte, TVariable<IntPtrT>* const var_index,
    const TNode<IntPtrT> end, int increment, Label* const if_none_found) {
  Label if_stringisonebyte(this), out(this);

  GotoIf(is_stringonebyte, &if_stringisonebyte);

  // Two Byte String
  BuildLoop<Uint16T>(
      var_index, end, increment, if_none_found, &out,
      [&](const TNode<IntPtrT> index) {
        return Load<Uint16T>(
            string_data,
            WordShl(IntPtrAdd(index, string_data_offset), IntPtrConstant(1)));
      });

  BIND(&if_stringisonebyte);
  BuildLoop<Uint8T>(var_index, end, increment, if_none_found, &out,
                    [&](const TNode<IntPtrT> index) {
                      return Load<Uint8T>(string_data,
                                          IntPtrAdd(index, string_data_offset));
                    });

  BIND(&out);
}

template <typename T>
void StringTrimAssembler::BuildLoop(
    TVariable<IntPtrT>* const var_index, const TNode<IntPtrT> end,
    int increment, Label* const if_none_found, Label* const out,
    const std::function<TNode<T>(const TNode<IntPtrT>)>& get_character) {
  Label loop(this, var_index);
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> index = var_index->value();
    GotoIf(IntPtrEqual(index, end), if_none_found);
    GotoIfNotWhiteSpaceOrLineTerminator(
        UncheckedCast<Uint32T>(get_character(index)), out);
    Increment(var_index, increment);
    Goto(&loop);
  }
}

void StringTrimAssembler::GotoIfNotWhiteSpaceOrLineTerminator(
    const TNode<Word32T> char_code, Label* const if_not_whitespace) {
  Label out(this);

  // 0x0020 - SPACE (Intentionally out of order to fast path a commmon case)
  GotoIf(Word32Equal(char_code, Int32Constant(0x0020)), &out);

  // 0x0009 - HORIZONTAL TAB
  GotoIf(Uint32LessThan(char_code, Int32Constant(0x0009)), if_not_whitespace);
  // 0x000A - LINE FEED OR NEW LINE
  // 0x000B - VERTICAL TAB
  // 0x000C - FORMFEED
  // 0x000D - HORIZONTAL TAB
  GotoIf(Uint32LessThanOrEqual(char_code, Int32Constant(0x000D)), &out);

  // Common Non-whitespace characters
  GotoIf(Uint32LessThan(char_code, Int32Constant(0x00A0)), if_not_whitespace);

  // 0x00A0 - NO-BREAK SPACE
  GotoIf(Word32Equal(char_code, Int32Constant(0x00A0)), &out);

  // 0x1680 - Ogham Space Mark
  GotoIf(Word32Equal(char_code, Int32Constant(0x1680)), &out);

  // 0x2000 - EN QUAD
  GotoIf(Uint32LessThan(char_code, Int32Constant(0x2000)), if_not_whitespace);
  // 0x2001 - EM QUAD
  // 0x2002 - EN SPACE
  // 0x2003 - EM SPACE
  // 0x2004 - THREE-PER-EM SPACE
  // 0x2005 - FOUR-PER-EM SPACE
  // 0x2006 - SIX-PER-EM SPACE
  // 0x2007 - FIGURE SPACE
  // 0x2008 - PUNCTUATION SPACE
  // 0x2009 - THIN SPACE
  // 0x200A - HAIR SPACE
  GotoIf(Uint32LessThanOrEqual(char_code, Int32Constant(0x200A)), &out);

  // 0x2028 - LINE SEPARATOR
  GotoIf(Word32Equal(char_code, Int32Constant(0x2028)), &out);
  // 0x2029 - PARAGRAPH SEPARATOR
  GotoIf(Word32Equal(char_code, Int32Constant(0x2029)), &out);
  // 0x202F - NARROW NO-BREAK SPACE
  GotoIf(Word32Equal(char_code, Int32Constant(0x202F)), &out);
  // 0x205F - MEDIUM MATHEMATICAL SPACE
  GotoIf(Word32Equal(char_code, Int32Constant(0x205F)), &out);
  // 0xFEFF - BYTE ORDER MARK
  GotoIf(Word32Equal(char_code, Int32Constant(0xFEFF)), &out);
  // 0x3000 - IDEOGRAPHIC SPACE
  Branch(Word32Equal(char_code, Int32Constant(0x3000)), &out,
         if_not_whitespace);

  BIND(&out);
}

// Return the |word32| codepoint at {index}. Supports SeqStrings and
// ExternalStrings.
// TODO(v8:9880): Use UintPtrT here.
TNode<Int32T> StringBuiltinsAssembler::LoadSurrogatePairAt(
    TNode<String> string, TNode<IntPtrT> length, TNode<IntPtrT> index,
    UnicodeEncoding encoding) {
  Label handle_surrogate_pair(this), return_result(this);
  TVARIABLE(Int32T, var_result);
  TVARIABLE(Int32T, var_trail);
  var_result = StringCharCodeAt(string, Unsigned(index));
  var_trail = Int32Constant(0);

  GotoIf(Word32NotEqual(Word32And(var_result.value(), Int32Constant(0xFC00)),
                        Int32Constant(0xD800)),
         &return_result);
  TNode<IntPtrT> next_index = IntPtrAdd(index, IntPtrConstant(1));

  GotoIfNot(IntPtrLessThan(next_index, length), &return_result);
  var_trail = StringCharCodeAt(string, Unsigned(next_index));
  Branch(Word32Equal(Word32And(var_trail.value(), Int32Constant(0xFC00)),
                     Int32Constant(0xDC00)),
         &handle_surrogate_pair, &return_result);

  BIND(&handle_surrogate_pair);
  {
    TNode<Int32T> lead = var_result.value();
    TNode<Int32T> trail = var_trail.value();

    // Check that this path is only taken if a surrogate pair is found
    CSA_SLOW_ASSERT(this,
                    Uint32GreaterThanOrEqual(lead, Int32Constant(0xD800)));
    CSA_SLOW_ASSERT(this, Uint32LessThan(lead, Int32Constant(0xDC00)));
    CSA_SLOW_ASSERT(this,
                    Uint32GreaterThanOrEqual(trail, Int32Constant(0xDC00)));
    CSA_SLOW_ASSERT(this, Uint32LessThan(trail, Int32Constant(0xE000)));

    switch (encoding) {
      case UnicodeEncoding::UTF16:
        var_result = Word32Or(
// Need to swap the order for big-endian platforms
#if V8_TARGET_BIG_ENDIAN
            Word32Shl(lead, Int32Constant(16)), trail);
#else
            Word32Shl(trail, Int32Constant(16)), lead);
#endif
        break;

      case UnicodeEncoding::UTF32: {
        // Convert UTF16 surrogate pair into |word32| code point, encoded as
        // UTF32.
        TNode<Int32T> surrogate_offset =
            Int32Constant(0x10000 - (0xD800 << 10) - 0xDC00);

        // (lead << 10) + trail + SURROGATE_OFFSET
        var_result = Int32Add(Word32Shl(lead, Int32Constant(10)),
                              Int32Add(trail, surrogate_offset));
        break;
      }
    }
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

void StringBuiltinsAssembler::BranchIfStringPrimitiveWithNoCustomIteration(
    TNode<Object> object, TNode<Context> context, Label* if_true,
    Label* if_false) {
  GotoIf(TaggedIsSmi(object), if_false);
  GotoIfNot(IsString(CAST(object)), if_false);

  // Check that the String iterator hasn't been modified in a way that would
  // affect iteration.
  TNode<PropertyCell> protector_cell = StringIteratorProtectorConstant();
  DCHECK(isolate()->heap()->string_iterator_protector().IsPropertyCell());
  Branch(
      TaggedEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Protectors::kProtectorValid)),
      if_true, if_false);
}

// Instantiate template due to shared library requirements.
template V8_EXPORT_PRIVATE void StringBuiltinsAssembler::CopyStringCharacters(
    TNode<String> from_string, TNode<String> to_string,
    TNode<IntPtrT> from_index, TNode<IntPtrT> to_index,
    TNode<IntPtrT> character_count, String::Encoding from_encoding,
    String::Encoding to_encoding);

template V8_EXPORT_PRIVATE void StringBuiltinsAssembler::CopyStringCharacters(
    TNode<RawPtrT> from_string, TNode<String> to_string,
    TNode<IntPtrT> from_index, TNode<IntPtrT> to_index,
    TNode<IntPtrT> character_count, String::Encoding from_encoding,
    String::Encoding to_encoding);

template <typename T>
void StringBuiltinsAssembler::CopyStringCharacters(
    TNode<T> from_string, TNode<String> to_string, TNode<IntPtrT> from_index,
    TNode<IntPtrT> to_index, TNode<IntPtrT> character_count,
    String::Encoding from_encoding, String::Encoding to_encoding) {
  // from_string could be either a String or a RawPtrT in the case we pass in
  // faked sequential strings when handling external subject strings.
  bool from_one_byte = from_encoding == String::ONE_BYTE_ENCODING;
  bool to_one_byte = to_encoding == String::ONE_BYTE_ENCODING;
  DCHECK_IMPLIES(to_one_byte, from_one_byte);
  Comment("CopyStringCharacters ",
          from_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING", " -> ",
          to_one_byte ? "ONE_BYTE_ENCODING" : "TWO_BYTE_ENCODING");

  ElementsKind from_kind = from_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  ElementsKind to_kind = to_one_byte ? UINT8_ELEMENTS : UINT16_ELEMENTS;
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  int header_size = SeqOneByteString::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> from_offset =
      ElementOffsetFromIndex(from_index, from_kind, header_size);
  TNode<IntPtrT> to_offset =
      ElementOffsetFromIndex(to_index, to_kind, header_size);
  TNode<IntPtrT> byte_count =
      ElementOffsetFromIndex(character_count, from_kind);
  TNode<IntPtrT> limit_offset = IntPtrAdd(from_offset, byte_count);

  // Prepare the fast loop
  MachineType type =
      from_one_byte ? MachineType::Uint8() : MachineType::Uint16();
  MachineRepresentation rep = to_one_byte ? MachineRepresentation::kWord8
                                          : MachineRepresentation::kWord16;
  int from_increment = 1 << ElementsKindToShiftSize(from_kind);
  int to_increment = 1 << ElementsKindToShiftSize(to_kind);

  TVARIABLE(IntPtrT, current_to_offset, to_offset);
  VariableList vars({&current_to_offset}, zone());
  int to_index_constant = 0, from_index_constant = 0;
  bool index_same = (from_encoding == to_encoding) &&
                    (from_index == to_index ||
                     (ToInt32Constant(from_index, &from_index_constant) &&
                      ToInt32Constant(to_index, &to_index_constant) &&
                      from_index_constant == to_index_constant));
  BuildFastLoop<IntPtrT>(
      vars, from_offset, limit_offset,
      [&](TNode<IntPtrT> offset) {
        StoreNoWriteBarrier(rep, to_string,
                            index_same ? offset : current_to_offset.value(),
                            Load(type, from_string, offset));
        if (!index_same) {
          Increment(&current_to_offset, to_increment);
        }
      },
      from_increment, IndexAdvanceMode::kPost);
}

// A wrapper around CopyStringCharacters which determines the correct string
// encoding, allocates a corresponding sequential string, and then copies the
// given character range using CopyStringCharacters.
// |from_string| must be a sequential string.
// 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
template <typename T>
TNode<String> StringBuiltinsAssembler::AllocAndCopyStringCharacters(
    TNode<T> from, TNode<Int32T> from_instance_type, TNode<IntPtrT> from_index,
    TNode<IntPtrT> character_count) {
  Label end(this), one_byte_sequential(this), two_byte_sequential(this);
  TVARIABLE(String, var_result);

  Branch(IsOneByteStringInstanceType(from_instance_type), &one_byte_sequential,
         &two_byte_sequential);

  // The subject string is a sequential one-byte string.
  BIND(&one_byte_sequential);
  {
    TNode<String> result = AllocateSeqOneByteString(
        Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters<T>(from, result, from_index, IntPtrConstant(0),
                            character_count, String::ONE_BYTE_ENCODING,
                            String::ONE_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  // The subject string is a sequential two-byte string.
  BIND(&two_byte_sequential);
  {
    TNode<String> result = AllocateSeqTwoByteString(
        Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters<T>(from, result, from_index, IntPtrConstant(0),
                            character_count, String::TWO_BYTE_ENCODING,
                            String::TWO_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

// TODO(v8:9880): Use UintPtrT here.
TNode<String> StringBuiltinsAssembler::SubString(TNode<String> string,
                                                 TNode<IntPtrT> from,
                                                 TNode<IntPtrT> to) {
  TVARIABLE(String, var_result);
  ToDirectStringAssembler to_direct(state(), string);
  Label end(this), runtime(this);

  const TNode<IntPtrT> substr_length = IntPtrSub(to, from);
  const TNode<IntPtrT> string_length = LoadStringLengthAsWord(string);

  // Begin dispatching based on substring length.

  Label original_string_or_invalid_length(this);
  GotoIf(UintPtrGreaterThanOrEqual(substr_length, string_length),
         &original_string_or_invalid_length);

  // A real substring (substr_length < string_length).
  Label empty(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(0)), &empty);

  Label single_char(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(1)), &single_char);

  // Deal with different string types: update the index if necessary
  // and extract the underlying string.

  TNode<String> direct_string = to_direct.TryToDirect(&runtime);
  TNode<IntPtrT> offset = IntPtrAdd(from, to_direct.offset());
  const TNode<Int32T> instance_type = to_direct.instance_type();

  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label external_string(this);
  {
    if (FLAG_string_slices) {
      Label next(this);

      // Short slice.  Copy instead of slicing.
      GotoIf(IntPtrLessThan(substr_length,
                            IntPtrConstant(SlicedString::kMinLength)),
             &next);

      // Allocate new sliced string.

      Counters* counters = isolate()->counters();
      IncrementCounter(counters->sub_string_native(), 1);

      Label one_byte_slice(this), two_byte_slice(this);
      Branch(IsOneByteStringInstanceType(to_direct.instance_type()),
             &one_byte_slice, &two_byte_slice);

      BIND(&one_byte_slice);
      {
        var_result = AllocateSlicedOneByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&two_byte_slice);
      {
        var_result = AllocateSlicedTwoByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&next);
    }

    // The subject string can only be external or sequential string of either
    // encoding at this point.
    GotoIf(to_direct.is_external(), &external_string);

    var_result = AllocAndCopyStringCharacters(direct_string, instance_type,
                                              offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Handle external string.
  BIND(&external_string);
  {
    const TNode<RawPtrT> fake_sequential_string =
        to_direct.PointerToString(&runtime);

    var_result = AllocAndCopyStringCharacters(
        fake_sequential_string, instance_type, offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  BIND(&empty);
  {
    var_result = EmptyStringConstant();
    Goto(&end);
  }

  // Substrings of length 1 are generated through CharCodeAt and FromCharCode.
  BIND(&single_char);
  {
    TNode<Int32T> char_code = StringCharCodeAt(string, Unsigned(from));
    var_result = StringFromSingleCharCode(char_code);
    Goto(&end);
  }

  BIND(&original_string_or_invalid_length);
  {
    CSA_ASSERT(this, IntPtrEqual(substr_length, string_length));

    // Equal length - check if {from, to} == {0, str.length}.
    GotoIf(UintPtrGreaterThan(from, IntPtrConstant(0)), &runtime);

    // Return the original string (substr_length == string_length).

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    var_result = string;
    Goto(&end);
  }

  // Fall back to a runtime call.
  BIND(&runtime);
  {
    var_result =
        CAST(CallRuntime(Runtime::kStringSubstring, NoContextConstant(), string,
                         SmiTag(from), SmiTag(to)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

}  // namespace internal
}  // namespace v8
