// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-string-gen.h"

#include "src/builtins/builtins-regexp-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/heap/factory-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
template <class T>
using TNode = compiler::TNode<T>;

Node* StringBuiltinsAssembler::DirectStringData(Node* string,
                                                Node* string_instance_type) {
  // Compute the effective offset of the first character.
  VARIABLE(var_data, MachineType::PointerRepresentation());
  Label if_sequential(this), if_external(this), if_join(this);
  Branch(Word32Equal(Word32And(string_instance_type,
                               Int32Constant(kStringRepresentationMask)),
                     Int32Constant(kSeqStringTag)),
         &if_sequential, &if_external);

  BIND(&if_sequential);
  {
    var_data.Bind(IntPtrAdd(
        IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag),
        BitcastTaggedToWord(string)));
    Goto(&if_join);
  }

  BIND(&if_external);
  {
    // This is only valid for ExternalStrings where the resource data
    // pointer is cached (i.e. no short external strings).
    CSA_ASSERT(
        this, Word32NotEqual(Word32And(string_instance_type,
                                       Int32Constant(kShortExternalStringMask)),
                             Int32Constant(kShortExternalStringTag)));
    var_data.Bind(LoadObjectField(string, ExternalString::kResourceDataOffset,
                                  MachineType::Pointer()));
    Goto(&if_join);
  }

  BIND(&if_join);
  return var_data.value();
}

void StringBuiltinsAssembler::DispatchOnStringEncodings(
    Node* const lhs_instance_type, Node* const rhs_instance_type,
    Label* if_one_one, Label* if_one_two, Label* if_two_one,
    Label* if_two_two) {
  STATIC_ASSERT(kStringEncodingMask == 0x8);
  STATIC_ASSERT(kTwoByteStringTag == 0x0);
  STATIC_ASSERT(kOneByteStringTag == 0x8);

  // First combine the encodings.

  Node* const encoding_mask = Int32Constant(kStringEncodingMask);
  Node* const lhs_encoding = Word32And(lhs_instance_type, encoding_mask);
  Node* const rhs_encoding = Word32And(rhs_instance_type, encoding_mask);

  Node* const combined_encodings =
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
Node* StringBuiltinsAssembler::CallSearchStringRaw(Node* const subject_ptr,
                                                   Node* const subject_length,
                                                   Node* const search_ptr,
                                                   Node* const search_length,
                                                   Node* const start_position) {
  Node* const function_addr = ExternalConstant(
      ExternalReference::search_string_raw<SubjectChar, PatternChar>());
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_intptr = MachineType::IntPtr();

  Node* const result = CallCFunction6(
      type_intptr, type_ptr, type_ptr, type_intptr, type_ptr, type_intptr,
      type_intptr, function_addr, isolate_ptr, subject_ptr, subject_length,
      search_ptr, search_length, start_position);

  return result;
}

Node* StringBuiltinsAssembler::PointerToStringDataAtIndex(
    Node* const string_data, Node* const index, String::Encoding encoding) {
  const ElementsKind kind = (encoding == String::ONE_BYTE_ENCODING)
                                ? UINT8_ELEMENTS
                                : UINT16_ELEMENTS;
  Node* const offset_in_bytes =
      ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS);
  return IntPtrAdd(string_data, offset_in_bytes);
}

void StringBuiltinsAssembler::GenerateStringEqual(Node* context, Node* left,
                                                  Node* right) {
  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);
  Label if_equal(this), if_notequal(this), if_indirect(this, Label::kDeferred),
      restart(this, {&var_left, &var_right});

  TNode<IntPtrT> lhs_length = LoadStringLengthAsWord(left);
  TNode<IntPtrT> rhs_length = LoadStringLengthAsWord(right);

  // Strings with different lengths cannot be equal.
  GotoIf(WordNotEqual(lhs_length, rhs_length), &if_notequal);

  Goto(&restart);
  BIND(&restart);
  Node* lhs = var_left.value();
  Node* rhs = var_right.value();

  Node* lhs_instance_type = LoadInstanceType(lhs);
  Node* rhs_instance_type = LoadInstanceType(rhs);

  StringEqual_Core(context, lhs, lhs_instance_type, rhs, rhs_instance_type,
                   lhs_length, &if_equal, &if_notequal, &if_indirect);

  BIND(&if_indirect);
  {
    // Try to unwrap indirect strings, restart the above attempt on success.
    MaybeDerefIndirectStrings(&var_left, lhs_instance_type, &var_right,
                              rhs_instance_type, &restart);

    TailCallRuntime(Runtime::kStringEqual, context, lhs, rhs);
  }

  BIND(&if_equal);
  Return(TrueConstant());

  BIND(&if_notequal);
  Return(FalseConstant());
}

void StringBuiltinsAssembler::StringEqual_Core(
    Node* context, Node* lhs, Node* lhs_instance_type, Node* rhs,
    Node* rhs_instance_type, TNode<IntPtrT> length, Label* if_equal,
    Label* if_not_equal, Label* if_indirect) {
  CSA_ASSERT(this, IsString(lhs));
  CSA_ASSERT(this, IsString(rhs));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(lhs), length));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(rhs), length));
  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  GotoIf(WordEqual(lhs, rhs), if_equal);

  // Combine the instance types into a single 16-bit value, so we can check
  // both of them at once.
  Node* both_instance_types = Word32Or(
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
  STATIC_ASSERT(kShortExternalStringTag != 0);
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  int const kBothDirectStringMask =
      kIsIndirectStringMask | kShortExternalStringMask |
      ((kIsIndirectStringMask | kShortExternalStringMask) << 8);
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
  Node* masked_instance_types =
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
    Node* lhs, Node* lhs_instance_type, MachineType lhs_type, Node* rhs,
    Node* rhs_instance_type, MachineType rhs_type, TNode<IntPtrT> length,
    Label* if_equal, Label* if_not_equal) {
  CSA_ASSERT(this, IsString(lhs));
  CSA_ASSERT(this, IsString(rhs));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(lhs), length));
  CSA_ASSERT(this, WordEqual(LoadStringLengthAsWord(rhs), length));

  // Compute the effective offset of the first character.
  Node* lhs_data = DirectStringData(lhs, lhs_instance_type);
  Node* rhs_data = DirectStringData(rhs, rhs_instance_type);

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
    Node* lhs_value =
        Load(lhs_type, lhs_data,
             WordShl(var_offset.value(),
                     ElementSizeLog2Of(lhs_type.representation())));
    Node* rhs_value =
        Load(rhs_type, rhs_data,
             WordShl(var_offset.value(),
                     ElementSizeLog2Of(rhs_type.representation())));

    // Check if the characters match.
    GotoIf(Word32NotEqual(lhs_value, rhs_value), if_not_equal);

    // Advance to next character.
    var_offset = IntPtrAdd(var_offset.value(), IntPtrConstant(1));
    Goto(&loop);
  }
}

void StringBuiltinsAssembler::Generate_StringAdd(StringAddFlags flags,
                                                 PretenureFlag pretenure_flag,
                                                 Node* context, Node* left,
                                                 Node* right) {
  switch (flags) {
    case STRING_ADD_CONVERT_LEFT: {
      // TODO(danno): The ToString and JSReceiverToPrimitive below could be
      // combined to avoid duplicate smi and instance type checks.
      left = ToString(context, JSReceiverToPrimitive(context, left));
      Callable callable = CodeFactory::StringAdd(
          isolate(), STRING_ADD_CHECK_NONE, pretenure_flag);
      TailCallStub(callable, context, left, right);
      break;
    }
    case STRING_ADD_CONVERT_RIGHT: {
      // TODO(danno): The ToString and JSReceiverToPrimitive below could be
      // combined to avoid duplicate smi and instance type checks.
      right = ToString(context, JSReceiverToPrimitive(context, right));
      Callable callable = CodeFactory::StringAdd(
          isolate(), STRING_ADD_CHECK_NONE, pretenure_flag);
      TailCallStub(callable, context, left, right);
      break;
    }
    case STRING_ADD_CHECK_NONE: {
      CodeStubAssembler::AllocationFlag allocation_flags =
          (pretenure_flag == TENURED) ? CodeStubAssembler::kPretenured
                                      : CodeStubAssembler::kNone;
      Return(StringAdd(context, CAST(left), CAST(right), allocation_flags));
      break;
    }
  }
}

TF_BUILTIN(StringAdd_CheckNone_NotTenured, StringBuiltinsAssembler) {
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);
  Generate_StringAdd(STRING_ADD_CHECK_NONE, NOT_TENURED, context, left, right);
}

TF_BUILTIN(StringAdd_CheckNone_Tenured, StringBuiltinsAssembler) {
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);
  Generate_StringAdd(STRING_ADD_CHECK_NONE, TENURED, context, left, right);
}

TF_BUILTIN(StringAdd_ConvertLeft_NotTenured, StringBuiltinsAssembler) {
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);
  Generate_StringAdd(STRING_ADD_CONVERT_LEFT, NOT_TENURED, context, left,
                     right);
}

TF_BUILTIN(StringAdd_ConvertRight_NotTenured, StringBuiltinsAssembler) {
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);
  Generate_StringAdd(STRING_ADD_CONVERT_RIGHT, NOT_TENURED, context, left,
                     right);
}

TF_BUILTIN(SubString, StringBuiltinsAssembler) {
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Smi> from = CAST(Parameter(Descriptor::kFrom));
  TNode<Smi> to = CAST(Parameter(Descriptor::kTo));
  Return(SubString(string, SmiUntag(from), SmiUntag(to)));
}

void StringBuiltinsAssembler::GenerateStringAt(char const* method_name,
                                               TNode<Context> context,
                                               Node* receiver,
                                               TNode<Object> maybe_position,
                                               TNode<Object> default_return,
                                               StringAtAccessor accessor) {
  // Check that {receiver} is coercible to Object and convert it to a String.
  TNode<String> string = ToThisString(context, receiver, method_name);

  // Convert the {position} to a Smi and check that it's in bounds of the
  // {string}.
  Label if_outofbounds(this, Label::kDeferred);
  TNode<Number> position = ToInteger_Inline(
      context, maybe_position, CodeStubAssembler::kTruncateMinusZero);
  GotoIfNot(TaggedIsSmi(position), &if_outofbounds);
  TNode<IntPtrT> index = SmiUntag(CAST(position));
  TNode<IntPtrT> length = LoadStringLengthAsWord(string);
  GotoIfNot(UintPtrLessThan(index, length), &if_outofbounds);
  TNode<Object> result = accessor(string, length, index);
  Return(result);

  BIND(&if_outofbounds);
  Return(default_return);
}

void StringBuiltinsAssembler::GenerateStringRelationalComparison(Node* context,
                                                                 Node* left,
                                                                 Node* right,
                                                                 Operation op) {
  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);

  Variable* input_vars[2] = {&var_left, &var_right};
  Label if_less(this), if_equal(this), if_greater(this);
  Label restart(this, 2, input_vars);
  Goto(&restart);
  BIND(&restart);

  Node* lhs = var_left.value();
  Node* rhs = var_right.value();
  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  GotoIf(WordEqual(lhs, rhs), &if_equal);

  // Load instance types of {lhs} and {rhs}.
  Node* lhs_instance_type = LoadInstanceType(lhs);
  Node* rhs_instance_type = LoadInstanceType(rhs);

  // Combine the instance types into a single 16-bit value, so we can check
  // both of them at once.
  Node* both_instance_types = Word32Or(
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
        Node* lhs_value = Load(MachineType::Uint8(), lhs, var_offset.value());
        Node* rhs_value = Load(MachineType::Uint8(), rhs, var_offset.value());

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
        TailCallRuntime(Runtime::kStringLessThan, context, lhs, rhs);
        break;
      case Operation::kLessThanOrEqual:
        TailCallRuntime(Runtime::kStringLessThanOrEqual, context, lhs, rhs);
        break;
      case Operation::kGreaterThan:
        TailCallRuntime(Runtime::kStringGreaterThan, context, lhs, rhs);
        break;
      case Operation::kGreaterThanOrEqual:
        TailCallRuntime(Runtime::kStringGreaterThanOrEqual, context, lhs, rhs);
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
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringEqual(context, left, right);
}

TF_BUILTIN(StringLessThan, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(context, left, right,
                                     Operation::kLessThan);
}

TF_BUILTIN(StringLessThanOrEqual, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(context, left, right,
                                     Operation::kLessThanOrEqual);
}

TF_BUILTIN(StringGreaterThan, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(context, left, right,
                                     Operation::kGreaterThan);
}

TF_BUILTIN(StringGreaterThanOrEqual, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(context, left, right,
                                     Operation::kGreaterThanOrEqual);
}

TF_BUILTIN(StringCharAt, StringBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);

  // Load the character code at the {position} from the {receiver}.
  TNode<Int32T> code = StringCharCodeAt(receiver, position);

  // And return the single character string with only that {code}
  TNode<String> result = StringFromSingleCharCode(code);
  Return(result);
}

TF_BUILTIN(StringCodePointAtUTF16, StringBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);
  // TODO(sigurds) Figure out if passing length as argument pays off.
  TNode<IntPtrT> length = LoadStringLengthAsWord(receiver);
  // Load the character code at the {position} from the {receiver}.
  TNode<Int32T> code =
      LoadSurrogatePairAt(receiver, length, position, UnicodeEncoding::UTF16);
  // And return it as TaggedSigned value.
  // TODO(turbofan): Allow builtins to return values untagged.
  TNode<Smi> result = SmiFromInt32(code);
  Return(result);
}

TF_BUILTIN(StringCodePointAtUTF32, StringBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);

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

// -----------------------------------------------------------------------------
// ES6 section 21.1 String Objects

// ES6 #sec-string.fromcharcode
TF_BUILTIN(StringFromCharCode, CodeStubAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  Node* context = Parameter(Descriptor::kContext);

  CodeStubArguments arguments(this, ChangeInt32ToIntPtr(argc));
  TNode<Smi> smi_argc = SmiTag(arguments.GetLength(INTPTR_PARAMETERS));
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
    Node* code = arguments.AtIndex(0);
    Node* code32 = TruncateTaggedToWord32(context, code);
    TNode<Int32T> code16 =
        Signed(Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit)));
    Node* result = StringFromSingleCharCode(code16);
    arguments.PopAndReturn(result);
  }

  Node* code16 = nullptr;
  BIND(&if_notoneargument);
  {
    Label two_byte(this);
    // Assume that the resulting string contains only one-byte characters.
    Node* one_byte_result = AllocateSeqOneByteString(context, smi_argc);

    TVARIABLE(IntPtrT, var_max_index);
    var_max_index = IntPtrConstant(0);

    // Iterate over the incoming arguments, converting them to 8-bit character
    // codes. Stop if any of the conversions generates a code that doesn't fit
    // in 8 bits.
    CodeStubAssembler::VariableList vars({&var_max_index}, zone());
    arguments.ForEach(vars, [this, context, &two_byte, &var_max_index, &code16,
                             one_byte_result](Node* arg) {
      Node* code32 = TruncateTaggedToWord32(context, arg);
      code16 = Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

      GotoIf(
          Int32GreaterThan(code16, Int32Constant(String::kMaxOneByteCharCode)),
          &two_byte);

      // The {code16} fits into the SeqOneByteString {one_byte_result}.
      Node* offset = ElementOffsetFromIndex(
          var_max_index.value(), UINT8_ELEMENTS,
          CodeStubAssembler::INTPTR_PARAMETERS,
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
    Node* two_byte_result = AllocateSeqTwoByteString(context, smi_argc);

    // Copy the characters that have already been put in the 8-bit string into
    // their corresponding positions in the new 16-bit string.
    TNode<IntPtrT> zero = IntPtrConstant(0);
    CopyStringCharacters(one_byte_result, two_byte_result, zero, zero,
                         var_max_index.value(), String::ONE_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING);

    // Write the character that caused the 8-bit to 16-bit fault.
    Node* max_index_offset =
        ElementOffsetFromIndex(var_max_index.value(), UINT16_ELEMENTS,
                               CodeStubAssembler::INTPTR_PARAMETERS,
                               SeqTwoByteString::kHeaderSize - kHeapObjectTag);
    StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                        max_index_offset, code16);
    var_max_index = IntPtrAdd(var_max_index.value(), IntPtrConstant(1));

    // Resume copying the passed-in arguments from the same place where the
    // 8-bit copy stopped, but this time copying over all of the characters
    // using a 16-bit representation.
    arguments.ForEach(
        vars,
        [this, context, two_byte_result, &var_max_index](Node* arg) {
          Node* code32 = TruncateTaggedToWord32(context, arg);
          Node* code16 =
              Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

          Node* offset = ElementOffsetFromIndex(
              var_max_index.value(), UINT16_ELEMENTS,
              CodeStubAssembler::INTPTR_PARAMETERS,
              SeqTwoByteString::kHeaderSize - kHeapObjectTag);
          StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                              offset, code16);
          var_max_index = IntPtrAdd(var_max_index.value(), IntPtrConstant(1));
        },
        var_max_index.value());

    arguments.PopAndReturn(two_byte_result);
  }
}

// ES6 #sec-string.prototype.charat
TF_BUILTIN(StringPrototypeCharAt, StringBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* receiver = Parameter(Descriptor::kReceiver);
  TNode<Object> maybe_position = CAST(Parameter(Descriptor::kPosition));

  GenerateStringAt("String.prototype.charAt", context, receiver, maybe_position,
                   EmptyStringConstant(),
                   [this](TNode<String> string, TNode<IntPtrT> length,
                          TNode<IntPtrT> index) {
                     TNode<Int32T> code = StringCharCodeAt(string, index);
                     return StringFromSingleCharCode(code);
                   });
}

// ES6 #sec-string.prototype.charcodeat
TF_BUILTIN(StringPrototypeCharCodeAt, StringBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* receiver = Parameter(Descriptor::kReceiver);
  TNode<Object> maybe_position = CAST(Parameter(Descriptor::kPosition));

  GenerateStringAt("String.prototype.charCodeAt", context, receiver,
                   maybe_position, NanConstant(),
                   [this](TNode<String> receiver, TNode<IntPtrT> length,
                          TNode<IntPtrT> index) {
                     Node* value = StringCharCodeAt(receiver, index);
                     return SmiFromInt32(value);
                   });
}

// ES6 #sec-string.prototype.codepointat
TF_BUILTIN(StringPrototypeCodePointAt, StringBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* receiver = Parameter(Descriptor::kReceiver);
  TNode<Object> maybe_position = CAST(Parameter(Descriptor::kPosition));

  GenerateStringAt("String.prototype.codePointAt", context, receiver,
                   maybe_position, UndefinedConstant(),
                   [this](TNode<String> receiver, TNode<IntPtrT> length,
                          TNode<IntPtrT> index) {
                     // This is always a call to a builtin from Javascript,
                     // so we need to produce UTF32.
                     Node* value = LoadSurrogatePairAt(receiver, length, index,
                                                       UnicodeEncoding::UTF32);
                     return SmiFromInt32(value);
                   });
}

// ES6 String.prototype.concat(...args)
// ES6 #sec-string.prototype.concat
TF_BUILTIN(StringPrototypeConcat, CodeStubAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  CodeStubArguments arguments(
      this,
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount)));
  Node* receiver = arguments.GetReceiver();
  Node* context = Parameter(Descriptor::kContext);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver = ToThisString(context, receiver, "String.prototype.concat");

  // Concatenate all the arguments passed to this builtin.
  VARIABLE(var_result, MachineRepresentation::kTagged);
  var_result.Bind(receiver);
  arguments.ForEach(
      CodeStubAssembler::VariableList({&var_result}, zone()),
      [this, context, &var_result](Node* arg) {
        arg = ToString_Inline(context, arg);
        var_result.Bind(CallStub(CodeFactory::StringAdd(isolate()), context,
                                 var_result.value(), arg));
      });
  arguments.PopAndReturn(var_result.value());
}

void StringBuiltinsAssembler::StringIndexOf(
    Node* const subject_string, Node* const search_string, Node* const position,
    std::function<void(Node*)> f_return) {
  CSA_ASSERT(this, IsString(subject_string));
  CSA_ASSERT(this, IsString(search_string));
  CSA_ASSERT(this, TaggedIsSmi(position));

  TNode<IntPtrT> const int_zero = IntPtrConstant(0);
  TNode<IntPtrT> const search_length = LoadStringLengthAsWord(search_string);
  TNode<IntPtrT> const subject_length = LoadStringLengthAsWord(subject_string);
  TNode<IntPtrT> const start_position = IntPtrMax(SmiUntag(position), int_zero);

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
  GotoIf(WordEqual(subject_string, search_string), &return_zero);

  // Try to unpack subject and search strings. Bail to runtime if either needs
  // to be flattened.
  ToDirectStringAssembler subject_to_direct(state(), subject_string);
  ToDirectStringAssembler search_to_direct(state(), search_string);

  Label call_runtime_unchecked(this, Label::kDeferred);

  subject_to_direct.TryToDirect(&call_runtime_unchecked);
  search_to_direct.TryToDirect(&call_runtime_unchecked);

  // Load pointers to string data.
  Node* const subject_ptr =
      subject_to_direct.PointerToData(&call_runtime_unchecked);
  Node* const search_ptr =
      search_to_direct.PointerToData(&call_runtime_unchecked);

  Node* const subject_offset = subject_to_direct.offset();
  Node* const search_offset = search_to_direct.offset();

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

  typedef const uint8_t onebyte_t;
  typedef const uc16 twobyte_t;

  BIND(&one_one);
  {
    Node* const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    Node* const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    Label direct_memchr_call(this), generic_fast_path(this);
    Branch(IntPtrEqual(search_length, IntPtrConstant(1)), &direct_memchr_call,
           &generic_fast_path);

    // An additional fast path that calls directly into memchr for 1-length
    // search strings.
    BIND(&direct_memchr_call);
    {
      Node* const string_addr = IntPtrAdd(adjusted_subject_ptr, start_position);
      Node* const search_length = IntPtrSub(subject_length, start_position);
      Node* const search_byte =
          ChangeInt32ToIntPtr(Load(MachineType::Uint8(), adjusted_search_ptr));

      Node* const memchr =
          ExternalConstant(ExternalReference::libc_memchr_function());
      Node* const result_address =
          CallCFunction3(MachineType::Pointer(), MachineType::Pointer(),
                         MachineType::IntPtr(), MachineType::UintPtr(), memchr,
                         string_addr, search_byte, search_length);
      GotoIf(WordEqual(result_address, int_zero), &return_minus_1);
      Node* const result_index =
          IntPtrAdd(IntPtrSub(result_address, string_addr), start_position);
      f_return(SmiTag(result_index));
    }

    BIND(&generic_fast_path);
    {
      Node* const result = CallSearchStringRaw<onebyte_t, onebyte_t>(
          adjusted_subject_ptr, subject_length, adjusted_search_ptr,
          search_length, start_position);
      f_return(SmiTag(result));
    }
  }

  BIND(&one_two);
  {
    Node* const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    Node* const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::TWO_BYTE_ENCODING);

    Node* const result = CallSearchStringRaw<onebyte_t, twobyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_one);
  {
    Node* const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    Node* const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    Node* const result = CallSearchStringRaw<twobyte_t, onebyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_two);
  {
    Node* const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    Node* const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::TWO_BYTE_ENCODING);

    Node* const result = CallSearchStringRaw<twobyte_t, twobyte_t>(
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
    Node* result =
        CallRuntime(Runtime::kStringIndexOfUnchecked, NoContextConstant(),
                    subject_string, search_string, position);
    f_return(result);
  }
}

// ES6 String.prototype.indexOf(searchString [, position])
// #sec-string.prototype.indexof
// Unchecked helper for builtins lowering.
TF_BUILTIN(StringIndexOf, StringBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* search_string = Parameter(Descriptor::kSearchString);
  Node* position = Parameter(Descriptor::kPosition);
  StringIndexOf(receiver, search_string, position,
                [this](Node* result) { this->Return(result); });
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
  Node* const receiver = arguments.GetReceiver();

  VARIABLE(var_search_string, MachineRepresentation::kTagged);
  VARIABLE(var_position, MachineRepresentation::kTagged);
  Label argc_1(this), argc_2(this), call_runtime(this, Label::kDeferred),
      fast_path(this);

  GotoIf(IntPtrEqual(argc, IntPtrConstant(1)), &argc_1);
  GotoIf(IntPtrGreaterThan(argc, IntPtrConstant(1)), &argc_2);
  {
    Comment("0 Argument case");
    CSA_ASSERT(this, IntPtrEqual(argc, IntPtrConstant(0)));
    Node* const undefined = UndefinedConstant();
    var_search_string.Bind(undefined);
    var_position.Bind(undefined);
    Goto(&call_runtime);
  }
  BIND(&argc_1);
  {
    Comment("1 Argument case");
    var_search_string.Bind(arguments.AtIndex(0));
    var_position.Bind(SmiConstant(0));
    Goto(&fast_path);
  }
  BIND(&argc_2);
  {
    Comment("2 Argument case");
    var_search_string.Bind(arguments.AtIndex(0));
    var_position.Bind(arguments.AtIndex(1));
    GotoIfNot(TaggedIsSmi(var_position.value()), &call_runtime);
    Goto(&fast_path);
  }
  BIND(&fast_path);
  {
    Comment("Fast Path");
    Node* const search = var_search_string.value();
    Node* const position = var_position.value();
    GotoIf(TaggedIsSmi(receiver), &call_runtime);
    GotoIf(TaggedIsSmi(search), &call_runtime);
    GotoIfNot(IsString(receiver), &call_runtime);
    GotoIfNot(IsString(search), &call_runtime);

    StringIndexOf(receiver, search, position, [&](Node* result) {
      CSA_ASSERT(this, TaggedIsSmi(result));
      arguments.PopAndReturn((variant == kIndexOf)
                                 ? result
                                 : SelectBooleanConstant(SmiGreaterThanOrEqual(
                                       CAST(result), SmiConstant(0))));
    });
  }
  BIND(&call_runtime);
  {
    Comment("Call Runtime");
    Runtime::FunctionId runtime = variant == kIndexOf
                                      ? Runtime::kStringIndexOf
                                      : Runtime::kStringIncludes;
    Node* const result =
        CallRuntime(runtime, context, receiver, var_search_string.value(),
                    var_position.value());
    arguments.PopAndReturn(result);
  }
}

void StringBuiltinsAssembler::RequireObjectCoercible(Node* const context,
                                                     Node* const value,
                                                     const char* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  Branch(IsNullOrUndefined(value), &throw_exception, &out);

  BIND(&throw_exception);
  ThrowTypeError(context, MessageTemplate::kCalledOnNullOrUndefined,
                 method_name);

  BIND(&out);
}

void StringBuiltinsAssembler::MaybeCallFunctionAtSymbol(
    Node* const context, Node* const object, Node* const maybe_string,
    Handle<Symbol> symbol, const NodeFunction0& regexp_call,
    const NodeFunction1& generic_call) {
  Label out(this);

  // Smis definitely don't have an attached symbol.
  GotoIf(TaggedIsSmi(object), &out);

  Node* const object_map = LoadMap(object);

  // Skip the slow lookup for Strings.
  {
    Label next(this);

    GotoIfNot(IsStringInstanceType(LoadMapInstanceType(object_map)), &next);

    Node* const native_context = LoadNativeContext(context);
    Node* const initial_proto_initial_map = LoadContextElement(
        native_context, Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX);

    Node* const string_fun =
        LoadContextElement(native_context, Context::STRING_FUNCTION_INDEX);
    Node* const initial_map =
        LoadObjectField(string_fun, JSFunction::kPrototypeOrInitialMapOffset);
    Node* const proto_map = LoadMap(LoadMapPrototype(initial_map));

    Branch(WordEqual(proto_map, initial_proto_initial_map), &out, &next);

    BIND(&next);
  }

  // Take the fast path for RegExps.
  // There's two conditions: {object} needs to be a fast regexp, and
  // {maybe_string} must be a string (we can't call ToString on the fast path
  // since it may mutate {object}).
  {
    Label stub_call(this), slow_lookup(this);

    GotoIf(TaggedIsSmi(maybe_string), &slow_lookup);
    GotoIfNot(IsString(maybe_string), &slow_lookup);

    RegExpBuiltinsAssembler regexp_asm(state());
    regexp_asm.BranchIfFastRegExp(context, object, object_map, &stub_call,
                                  &slow_lookup);

    BIND(&stub_call);
    // TODO(jgruber): Add a no-JS scope once it exists.
    regexp_call();

    BIND(&slow_lookup);
  }

  GotoIf(IsNullOrUndefined(object), &out);

  // Fall back to a slow lookup of {object[symbol]}.
  //
  // The spec uses GetMethod({object}, {symbol}), which has a few quirks:
  // * null values are turned into undefined, and
  // * an exception is thrown if the value is not undefined, null, or callable.
  // We handle the former by jumping to {out} for null values as well, while
  // the latter is already handled by the Call({maybe_func}) operation.

  Node* const maybe_func = GetProperty(context, object, symbol);
  GotoIf(IsUndefined(maybe_func), &out);
  GotoIf(IsNull(maybe_func), &out);

  // Attempt to call the function.
  generic_call(maybe_func);

  BIND(&out);
}

TNode<Smi> StringBuiltinsAssembler::IndexOfDollarChar(Node* const context,
                                                      Node* const string) {
  CSA_ASSERT(this, IsString(string));

  TNode<String> const dollar_string = HeapConstant(
      isolate()->factory()->LookupSingleCharacterStringFromCode('$'));
  TNode<Smi> const dollar_ix =
      CAST(CallBuiltin(Builtins::kStringIndexOf, context, string, dollar_string,
                       SmiConstant(0)));
  return dollar_ix;
}

compiler::Node* StringBuiltinsAssembler::GetSubstitution(
    Node* context, Node* subject_string, Node* match_start_index,
    Node* match_end_index, Node* replace_string) {
  CSA_ASSERT(this, IsString(subject_string));
  CSA_ASSERT(this, IsString(replace_string));
  CSA_ASSERT(this, TaggedIsPositiveSmi(match_start_index));
  CSA_ASSERT(this, TaggedIsPositiveSmi(match_end_index));

  VARIABLE(var_result, MachineRepresentation::kTagged, replace_string);
  Label runtime(this), out(this);

  // In this primitive implementation we simply look for the next '$' char in
  // {replace_string}. If it doesn't exist, we can simply return
  // {replace_string} itself. If it does, then we delegate to
  // String::GetSubstitution, passing in the index of the first '$' to avoid
  // repeated scanning work.
  // TODO(jgruber): Possibly extend this in the future to handle more complex
  // cases without runtime calls.

  TNode<Smi> const dollar_index = IndexOfDollarChar(context, replace_string);
  Branch(SmiIsNegative(dollar_index), &out, &runtime);

  BIND(&runtime);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(dollar_index));

    Node* const matched =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_start_index), SmiUntag(match_end_index));
    Node* const replacement_string =
        CallRuntime(Runtime::kGetSubstitution, context, matched, subject_string,
                    match_start_index, replace_string, dollar_index);
    var_result.Bind(replacement_string);

    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES6 #sec-string.prototype.repeat
TF_BUILTIN(StringPrototypeRepeat, StringBuiltinsAssembler) {
  Label invalid_count(this), invalid_string_length(this),
      return_emptystring(this);

  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Node* const receiver = Parameter(Descriptor::kReceiver);
  TNode<Object> count = CAST(Parameter(Descriptor::kCount));
  Node* const string =
      ToThisString(context, receiver, "String.prototype.repeat");
  Node* const is_stringempty =
      SmiEqual(LoadStringLengthAsSmi(string), SmiConstant(0));

  VARIABLE(
      var_count, MachineRepresentation::kTagged,
      ToInteger_Inline(context, count, CodeStubAssembler::kTruncateMinusZero));

  // Verifies a valid count and takes a fast path when the result will be an
  // empty string.
  {
    Label if_count_isheapnumber(this, Label::kDeferred);

    GotoIfNot(TaggedIsSmi(var_count.value()), &if_count_isheapnumber);
    {
      // If count is a SMI, throw a RangeError if less than 0 or greater than
      // the maximum string length.
      TNode<Smi> smi_count = CAST(var_count.value());
      GotoIf(SmiLessThan(smi_count, SmiConstant(0)), &invalid_count);
      GotoIf(SmiEqual(smi_count, SmiConstant(0)), &return_emptystring);
      GotoIf(is_stringempty, &return_emptystring);
      GotoIf(SmiGreaterThan(smi_count, SmiConstant(String::kMaxLength)),
             &invalid_string_length);
      Return(CallBuiltin(Builtins::kStringRepeat, context, string, smi_count));
    }

    // If count is a Heap Number...
    // 1) If count is Infinity, throw a RangeError exception
    // 2) If receiver is an empty string, return an empty string
    // 3) Otherwise, throw RangeError exception
    BIND(&if_count_isheapnumber);
    {
      CSA_ASSERT(this, IsNumberNormalized(var_count.value()));
      Node* const number_value = LoadHeapNumberValue(var_count.value());
      GotoIf(Float64Equal(number_value, Float64Constant(V8_INFINITY)),
             &invalid_count);
      GotoIf(Float64LessThan(number_value, Float64Constant(0.0)),
             &invalid_count);
      Branch(is_stringempty, &return_emptystring, &invalid_string_length);
    }
  }

  BIND(&return_emptystring);
  Return(EmptyStringConstant());

  BIND(&invalid_count);
  {
    ThrowRangeError(context, MessageTemplate::kInvalidCountValue,
                    var_count.value());
  }

  BIND(&invalid_string_length);
  {
    CallRuntime(Runtime::kThrowInvalidStringLength, context);
    Unreachable();
  }
}

// Helper with less checks
TF_BUILTIN(StringRepeat, StringBuiltinsAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const string = Parameter(Descriptor::kString);
  TNode<Smi> const count = CAST(Parameter(Descriptor::kCount));

  CSA_ASSERT(this, IsString(string));
  CSA_ASSERT(this, Word32BinaryNot(IsEmptyString(string)));
  CSA_ASSERT(this, TaggedIsPositiveSmi(count));
  CSA_ASSERT(this, SmiLessThanOrEqual(count, SmiConstant(String::kMaxLength)));

  // The string is repeated with the following algorithm:
  //   let n = count;
  //   let power_of_two_repeats = string;
  //   let result = "";
  //   while (true) {
  //     if (n & 1) result += s;
  //     n >>= 1;
  //     if (n === 0) return result;
  //     power_of_two_repeats += power_of_two_repeats;
  //   }
  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());
  VARIABLE(var_temp, MachineRepresentation::kTagged, string);
  TVARIABLE(Smi, var_count, count);

  Callable stringadd_callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  Label loop(this, {&var_count, &var_result, &var_temp}), return_result(this);
  Goto(&loop);
  BIND(&loop);
  {
    {
      Label next(this);
      GotoIfNot(SmiToInt32(SmiAnd(var_count.value(), SmiConstant(1))), &next);
      var_result.Bind(CallStub(stringadd_callable, context, var_result.value(),
                               var_temp.value()));
      Goto(&next);
      BIND(&next);
    }

    var_count = SmiShr(var_count.value(), 1);
    GotoIf(SmiEqual(var_count.value(), SmiConstant(0)), &return_result);
    var_temp.Bind(CallStub(stringadd_callable, context, var_temp.value(),
                           var_temp.value()));
    Goto(&loop);
  }

  BIND(&return_result);
  Return(var_result.value());
}

// ES6 #sec-string.prototype.replace
TF_BUILTIN(StringPrototypeReplace, StringBuiltinsAssembler) {
  Label out(this);

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const search = Parameter(Descriptor::kSearch);
  Node* const replace = Parameter(Descriptor::kReplace);
  Node* const context = Parameter(Descriptor::kContext);

  TNode<Smi> const smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.replace");

  // Redirect to replacer method if {search[@@replace]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, search, receiver, isolate()->factory()->replace_symbol(),
      [=]() {
        Return(CallBuiltin(Builtins::kRegExpReplace, context, search, receiver,
                           replace));
      },
      [=](Node* fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        Return(CallJS(call_callable, context, fn, search, receiver, replace));
      });

  // Convert {receiver} and {search} to strings.

  TNode<String> const subject_string = ToString_Inline(context, receiver);
  TNode<String> const search_string = ToString_Inline(context, search);

  TNode<Smi> const subject_length = LoadStringLengthAsSmi(subject_string);
  TNode<Smi> const search_length = LoadStringLengthAsSmi(search_string);

  // Fast-path single-char {search}, long cons {receiver}, and simple string
  // {replace}.
  {
    Label next(this);

    GotoIfNot(SmiEqual(search_length, SmiConstant(1)), &next);
    GotoIfNot(SmiGreaterThan(subject_length, SmiConstant(0xFF)), &next);
    GotoIf(TaggedIsSmi(replace), &next);
    GotoIfNot(IsString(replace), &next);

    Node* const subject_instance_type = LoadInstanceType(subject_string);
    GotoIfNot(IsConsStringInstanceType(subject_instance_type), &next);

    GotoIf(TaggedIsPositiveSmi(IndexOfDollarChar(context, replace)), &next);

    // Searching by traversing a cons string tree and replace with cons of
    // slices works only when the replaced string is a single character, being
    // replaced by a simple string and only pays off for long strings.
    // TODO(jgruber): Reevaluate if this is still beneficial.
    // TODO(jgruber): TailCallRuntime when it correctly handles adapter frames.
    Return(CallRuntime(Runtime::kStringReplaceOneCharWithString, context,
                       subject_string, search_string, replace));

    BIND(&next);
  }

  // TODO(jgruber): Extend StringIndexOf to handle two-byte strings and
  // longer substrings - we can handle up to 8 chars (one-byte) / 4 chars
  // (2-byte).

  TNode<Smi> const match_start_index =
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
    GotoIf(IsCallableMap(LoadMap(replace)), &return_subject);

    // TODO(jgruber): Could introduce ToStringSideeffectsStub which only
    // performs observable parts of ToString.
    ToString_Inline(context, replace);
    Goto(&return_subject);

    BIND(&return_subject);
    Return(subject_string);

    BIND(&next);
  }

  TNode<Smi> const match_end_index = SmiAdd(match_start_index, search_length);

  Callable stringadd_callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  // Compute the prefix.
  {
    Label next(this);

    GotoIf(SmiEqual(match_start_index, smi_zero), &next);
    Node* const prefix =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    IntPtrConstant(0), SmiUntag(match_start_index));
    var_result.Bind(prefix);

    Goto(&next);
    BIND(&next);
  }

  // Compute the string to replace with.

  Label if_iscallablereplace(this), if_notcallablereplace(this);
  GotoIf(TaggedIsSmi(replace), &if_notcallablereplace);
  Branch(IsCallableMap(LoadMap(replace)), &if_iscallablereplace,
         &if_notcallablereplace);

  BIND(&if_iscallablereplace);
  {
    Callable call_callable = CodeFactory::Call(isolate());
    Node* const replacement =
        CallJS(call_callable, context, replace, UndefinedConstant(),
               search_string, match_start_index, subject_string);
    Node* const replacement_string = ToString_Inline(context, replacement);
    var_result.Bind(CallStub(stringadd_callable, context, var_result.value(),
                             replacement_string));
    Goto(&out);
  }

  BIND(&if_notcallablereplace);
  {
    Node* const replace_string = ToString_Inline(context, replace);
    Node* const replacement =
        GetSubstitution(context, subject_string, match_start_index,
                        match_end_index, replace_string);
    var_result.Bind(
        CallStub(stringadd_callable, context, var_result.value(), replacement));
    Goto(&out);
  }

  BIND(&out);
  {
    Node* const suffix =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_end_index), SmiUntag(subject_length));
    Node* const result =
        CallStub(stringadd_callable, context, var_result.value(), suffix);
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
    if (variant == kMatch) {
      builtin = Builtins::kRegExpMatchFast;
      symbol = isolate()->factory()->match_symbol();
    } else {
      builtin = Builtins::kRegExpSearchFast;
      symbol = isolate()->factory()->search_symbol();
    }

    RequireObjectCoercible(context, receiver, method_name);

    MaybeCallFunctionAtSymbol(
        context, maybe_regexp, receiver, symbol,
        [=] { Return(CallBuiltin(builtin, context, maybe_regexp, receiver)); },
        [=](Node* fn) {
          Callable call_callable = CodeFactory::Call(isolate());
          Return(CallJS(call_callable, context, fn, maybe_regexp, receiver));
        });

    // maybe_regexp is not a RegExp nor has [@@match / @@search] property.
    {
      RegExpBuiltinsAssembler regexp_asm(state());

      TNode<String> receiver_string = ToString_Inline(context, receiver);
      TNode<Context> native_context = LoadNativeContext(context);
      TNode<HeapObject> regexp_function = CAST(
          LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX));
      TNode<Map> initial_map = CAST(LoadObjectField(
          regexp_function, JSFunction::kPrototypeOrInitialMapOffset));
      TNode<Object> regexp = regexp_asm.RegExpCreate(
          context, initial_map, maybe_regexp, EmptyStringConstant());

      Label fast_path(this), slow_path(this);
      regexp_asm.BranchIfFastRegExp(context, regexp, initial_map, &fast_path,
                                    &slow_path);

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
  TNode<Context> native_context = LoadNativeContext(context);

  // 1. Let O be ? RequireObjectCoercible(this value).
  RequireObjectCoercible(context, receiver, method_name);

  // 2. If regexp is neither undefined nor null, then
  Label return_match_all_iterator(this),
      tostring_and_return_match_all_iterator(this, Label::kDeferred);
  TVARIABLE(BoolT, var_is_fast_regexp);
  TVARIABLE(String, var_receiver_string);
  GotoIf(IsNullOrUndefined(maybe_regexp),
         &tostring_and_return_match_all_iterator);
  {
    // a. Let matcher be ? GetMethod(regexp, @@matchAll).
    // b. If matcher is not undefined, then
    //   i. Return ? Call(matcher, regexp, « O »).
    auto if_regexp_call = [&] {
      // MaybeCallFunctionAtSymbol guarantees fast path is chosen only if
      // maybe_regexp is a fast regexp and receiver is a string.
      var_receiver_string = CAST(receiver);
      CSA_ASSERT(this, IsString(var_receiver_string.value()));
      var_is_fast_regexp = Int32TrueConstant();
      Goto(&return_match_all_iterator);
    };
    auto if_generic_call = [=](Node* fn) {
      Callable call_callable = CodeFactory::Call(isolate());
      Return(CallJS(call_callable, context, fn, maybe_regexp, receiver));
    };
    MaybeCallFunctionAtSymbol(context, maybe_regexp, receiver,
                              isolate()->factory()->match_all_symbol(),
                              if_regexp_call, if_generic_call);
    Goto(&tostring_and_return_match_all_iterator);
  }
  BIND(&tostring_and_return_match_all_iterator);
  {
    var_receiver_string = ToString_Inline(context, receiver);
    var_is_fast_regexp = Int32FalseConstant();
    Goto(&return_match_all_iterator);
  }
  BIND(&return_match_all_iterator);
  {
    // 3. Return ? MatchAllIterator(regexp, O).
    RegExpBuiltinsAssembler regexp_asm(state());
    TNode<Object> iterator = regexp_asm.MatchAllIterator(
        context, native_context, maybe_regexp, var_receiver_string.value(),
        var_is_fast_regexp.value(), method_name);
    Return(iterator);
  }
}

class StringPadAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringPadAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

 protected:
  enum Variant { kStart, kEnd };

  void Generate(Variant variant, const char* method_name, TNode<IntPtrT> argc,
                TNode<Context> context) {
    CodeStubArguments arguments(this, argc);
    Node* const receiver = arguments.GetReceiver();
    Node* const receiver_string = ToThisString(context, receiver, method_name);
    TNode<Smi> const string_length = LoadStringLengthAsSmi(receiver_string);

    TVARIABLE(String, var_fill_string, StringConstant(" "));
    TVARIABLE(IntPtrT, var_fill_length, IntPtrConstant(1));

    Label check_fill(this), dont_pad(this), invalid_string_length(this),
        pad(this);

    // If no max_length was provided, return the string.
    GotoIf(IntPtrEqual(argc, IntPtrConstant(0)), &dont_pad);

    TNode<Number> const max_length =
        ToLength_Inline(context, arguments.AtIndex(0));
    CSA_ASSERT(this, IsNumberNormalized(max_length));

    // If max_length <= string_length, return the string.
    GotoIfNot(TaggedIsSmi(max_length), &check_fill);
    Branch(SmiLessThanOrEqual(CAST(max_length), string_length), &dont_pad,
           &check_fill);

    BIND(&check_fill);
    {
      GotoIf(IntPtrEqual(argc, IntPtrConstant(1)), &pad);
      Node* const fill = arguments.AtIndex(1);
      GotoIf(IsUndefined(fill), &pad);

      var_fill_string = ToString_Inline(context, fill);
      var_fill_length = LoadStringLengthAsWord(var_fill_string.value());
      Branch(WordEqual(var_fill_length.value(), IntPtrConstant(0)), &dont_pad,
             &pad);
    }

    BIND(&pad);
    {
      CSA_ASSERT(this,
                 IntPtrGreaterThan(var_fill_length.value(), IntPtrConstant(0)));

      // Throw if max_length is greater than String::kMaxLength.
      GotoIfNot(TaggedIsSmi(max_length), &invalid_string_length);
      TNode<Smi> smi_max_length = CAST(max_length);
      GotoIfNot(
          SmiLessThanOrEqual(smi_max_length, SmiConstant(String::kMaxLength)),
          &invalid_string_length);

      Callable stringadd_callable =
          CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
      CSA_ASSERT(this, SmiGreaterThan(smi_max_length, string_length));
      TNode<Smi> const pad_length = SmiSub(smi_max_length, string_length);

      VARIABLE(var_pad, MachineRepresentation::kTagged);
      Label single_char_fill(this), multi_char_fill(this), return_result(this);
      Branch(IntPtrEqual(var_fill_length.value(), IntPtrConstant(1)),
             &single_char_fill, &multi_char_fill);

      // Fast path for a single character fill.  No need to calculate number of
      // repetitions or remainder.
      BIND(&single_char_fill);
      {
        var_pad.Bind(CallBuiltin(Builtins::kStringRepeat, context,
                                 static_cast<Node*>(var_fill_string.value()),
                                 pad_length));
        Goto(&return_result);
      }
      BIND(&multi_char_fill);
      {
        TNode<Int32T> const fill_length_word32 =
            TruncateIntPtrToInt32(var_fill_length.value());
        TNode<Int32T> const pad_length_word32 = SmiToInt32(pad_length);
        TNode<Int32T> const repetitions_word32 =
            Int32Div(pad_length_word32, fill_length_word32);
        TNode<Int32T> const remaining_word32 =
            Int32Mod(pad_length_word32, fill_length_word32);

        var_pad.Bind(CallBuiltin(Builtins::kStringRepeat, context,
                                 var_fill_string.value(),
                                 SmiFromInt32(repetitions_word32)));

        GotoIfNot(remaining_word32, &return_result);
        {
          Node* const remainder_string = CallBuiltin(
              Builtins::kStringSubstring, context, var_fill_string.value(),
              IntPtrConstant(0), ChangeInt32ToIntPtr(remaining_word32));
          var_pad.Bind(CallStub(stringadd_callable, context, var_pad.value(),
                                remainder_string));
          Goto(&return_result);
        }
      }
      BIND(&return_result);
      CSA_ASSERT(this,
                 SmiEqual(pad_length, LoadStringLengthAsSmi(var_pad.value())));
      arguments.PopAndReturn(variant == kStart
                                 ? CallStub(stringadd_callable, context,
                                            var_pad.value(), receiver_string)
                                 : CallStub(stringadd_callable, context,
                                            receiver_string, var_pad.value()));
    }
    BIND(&dont_pad);
    arguments.PopAndReturn(receiver_string);
    BIND(&invalid_string_length);
    {
      CallRuntime(Runtime::kThrowInvalidStringLength, context);
      Unreachable();
    }
  }
};

TF_BUILTIN(StringPrototypePadEnd, StringPadAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(kEnd, "String.prototype.padEnd", argc, context);
}

TF_BUILTIN(StringPrototypePadStart, StringPadAssembler) {
  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Generate(kStart, "String.prototype.padStart", argc, context);
}

// ES6 #sec-string.prototype.search
TF_BUILTIN(StringPrototypeSearch, StringMatchSearchAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_regexp = CAST(Parameter(Descriptor::kRegexp));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  Generate(kSearch, "String.prototype.search", receiver, maybe_regexp, context);
}

// ES6 section 21.1.3.18 String.prototype.slice ( start, end )
TF_BUILTIN(StringPrototypeSlice, StringBuiltinsAssembler) {
  Label out(this);
  TVARIABLE(IntPtrT, var_start);
  TVARIABLE(IntPtrT, var_end);

  const int kStart = 0;
  const int kEnd = 1;
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* const receiver = args.GetReceiver();
  TNode<Object> start = args.GetOptionalArgumentValue(kStart);
  TNode<Object> end = args.GetOptionalArgumentValue(kEnd);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 1. Let O be ? RequireObjectCoercible(this value).
  RequireObjectCoercible(context, receiver, "String.prototype.slice");

  // 2. Let S be ? ToString(O).
  TNode<String> const subject_string =
      CAST(CallBuiltin(Builtins::kToString, context, receiver));

  // 3. Let len be the number of elements in S.
  TNode<IntPtrT> const length = LoadStringLengthAsWord(subject_string);

  // Convert {start} to a relative index.
  var_start = ConvertToRelativeIndex(context, start, length);

  // 5. If end is undefined, let intEnd be len;
  var_end = length;
  GotoIf(IsUndefined(end), &out);

  // Convert {end} to a relative index.
  var_end = ConvertToRelativeIndex(context, end, length);
  Goto(&out);

  Label return_emptystring(this);
  BIND(&out);
  {
    GotoIf(IntPtrLessThanOrEqual(var_end.value(), var_start.value()),
           &return_emptystring);
    TNode<String> const result =
        SubString(subject_string, var_start.value(), var_end.value());
    args.PopAndReturn(result);
  }

  BIND(&return_emptystring);
  args.PopAndReturn(EmptyStringConstant());
}

TNode<JSArray> StringBuiltinsAssembler::StringToArray(
    TNode<Context> context, TNode<String> subject_string,
    TNode<Smi> subject_length, TNode<Number> limit_number) {
  CSA_ASSERT(this, SmiGreaterThan(subject_length, SmiConstant(0)));

  Label done(this), call_runtime(this, Label::kDeferred),
      fill_thehole_and_call_runtime(this, Label::kDeferred);
  TVARIABLE(JSArray, result_array);

  TNode<Int32T> instance_type = LoadInstanceType(subject_string);
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
    TNode<RawPtrT> string_data = UncheckedCast<RawPtrT>(
        to_direct.PointerToData(&fill_thehole_and_call_runtime));
    TNode<IntPtrT> string_data_offset = to_direct.offset();
    TNode<Object> cache = LoadRoot(Heap::kSingleCharacterStringCacheRootIndex);

    BuildFastLoop(
        IntPtrConstant(0), length,
        [&](Node* index) {
          // TODO(jkummerow): Implement a CSA version of DisallowHeapAllocation
          // and use that to guard ToDirectStringAssembler.PointerToData().
          CSA_ASSERT(this, WordEqual(to_direct.PointerToData(&call_runtime),
                                     string_data));
          TNode<Int32T> char_code =
              UncheckedCast<Int32T>(Load(MachineType::Uint8(), string_data,
                                         IntPtrAdd(index, string_data_offset)));
          Node* code_index = ChangeUint32ToWord(char_code);
          TNode<Object> entry = LoadFixedArrayElement(CAST(cache), code_index);

          // If we cannot find a char in the cache, fill the hole for the fixed
          // array, and call runtime.
          GotoIf(IsUndefined(entry), &fill_thehole_and_call_runtime);

          StoreFixedArrayElement(elements, index, entry);
        },
        1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

    TNode<Map> array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, context);
    result_array = CAST(
        AllocateUninitializedJSArrayWithoutElements(array_map, length_smi));
    StoreObjectField(result_array.value(), JSObject::kElementsOffset, elements);
    Goto(&done);

    BIND(&fill_thehole_and_call_runtime);
    {
      FillFixedArrayWithValue(PACKED_ELEMENTS, elements, IntPtrConstant(0),
                              length, Heap::kTheHoleValueRootIndex);
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

  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* const receiver = args.GetReceiver();
  Node* const separator = args.GetOptionalArgumentValue(kSeparatorArg);
  Node* const limit = args.GetOptionalArgumentValue(kLimitArg);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TNode<Smi> smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.split");

  // Redirect to splitter method if {separator[@@split]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, separator, receiver, isolate()->factory()->split_symbol(),
      [&]() {
        args.PopAndReturn(CallBuiltin(Builtins::kRegExpSplit, context,
                                      separator, receiver, limit));
      },
      [&](Node* fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        args.PopAndReturn(
            CallJS(call_callable, context, fn, separator, receiver, limit));
      });

  // String and integer conversions.

  TNode<String> subject_string = ToString_Inline(context, receiver);
  TNode<Number> limit_number = Select<Number>(
      IsUndefined(limit), [=] { return NumberConstant(kMaxUInt32); },
      [=] { return ToUint32(context, limit); });
  Node* const separator_string = ToString_Inline(context, separator);

  Label return_empty_array(this);

  // Shortcut for {limit} == 0.
  GotoIf(WordEqual<Object, Object>(limit_number, smi_zero),
         &return_empty_array);

  // ECMA-262 says that if {separator} is undefined, the result should
  // be an array of size 1 containing the entire string.
  {
    Label next(this);
    GotoIfNot(IsUndefined(separator), &next);

    const ElementsKind kind = PACKED_ELEMENTS;
    Node* const native_context = LoadNativeContext(context);
    Node* const array_map = LoadJSArrayElementsMap(kind, native_context);

    Node* const length = SmiConstant(1);
    Node* const capacity = IntPtrConstant(1);
    Node* const result = AllocateJSArray(kind, array_map, capacity, length);

    TNode<FixedArray> const fixed_array = CAST(LoadElements(result));
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

  Node* const result =
      CallRuntime(Runtime::kStringSplit, context, subject_string,
                  separator_string, limit_number);
  args.PopAndReturn(result);

  BIND(&return_empty_array);
  {
    const ElementsKind kind = PACKED_ELEMENTS;
    Node* const native_context = LoadNativeContext(context);
    Node* const array_map = LoadJSArrayElementsMap(kind, native_context);

    Node* const length = smi_zero;
    Node* const capacity = IntPtrConstant(0);
    Node* const result = AllocateJSArray(kind, array_map, capacity, length);

    args.PopAndReturn(result);
  }
}

// ES6 #sec-string.prototype.substr
TF_BUILTIN(StringPrototypeSubstr, StringBuiltinsAssembler) {
  const int kStartArg = 0;
  const int kLengthArg = 1;

  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* const receiver = args.GetReceiver();
  TNode<Object> start = args.GetOptionalArgumentValue(kStartArg);
  TNode<Object> length = args.GetOptionalArgumentValue(kLengthArg);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label out(this);

  TVARIABLE(IntPtrT, var_start);
  TVARIABLE(Number, var_length);

  TNode<IntPtrT> const zero = IntPtrConstant(0);

  // Check that {receiver} is coercible to Object and convert it to a String.
  TNode<String> const string =
      ToThisString(context, receiver, "String.prototype.substr");

  TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);

  // Convert {start} to a relative index.
  var_start = ConvertToRelativeIndex(context, start, string_length);

  // Conversions and bounds-checks for {length}.
  Label if_issmi(this), if_isheapnumber(this, Label::kDeferred);

  // Default to {string_length} if {length} is undefined.
  {
    Label if_isundefined(this, Label::kDeferred), if_isnotundefined(this);
    Branch(IsUndefined(length), &if_isundefined, &if_isnotundefined);

    BIND(&if_isundefined);
    var_length = SmiTag(string_length);
    Goto(&if_issmi);

    BIND(&if_isnotundefined);
    var_length = ToInteger_Inline(context, length,
                                  CodeStubAssembler::kTruncateMinusZero);
  }

  TVARIABLE(IntPtrT, var_result_length);

  Branch(TaggedIsSmi(var_length.value()), &if_issmi, &if_isheapnumber);

  // Set {length} to min(max({length}, 0), {string_length} - {start}
  BIND(&if_issmi);
  {
    TNode<IntPtrT> const positive_length =
        IntPtrMax(SmiUntag(CAST(var_length.value())), zero);
    TNode<IntPtrT> const minimal_length =
        IntPtrSub(string_length, var_start.value());
    var_result_length = IntPtrMin(positive_length, minimal_length);

    GotoIfNot(IntPtrLessThanOrEqual(var_result_length.value(), zero), &out);
    args.PopAndReturn(EmptyStringConstant());
  }

  BIND(&if_isheapnumber);
  {
    // If {length} is a heap number, it is definitely out of bounds. There are
    // two cases according to the spec: if it is negative, "" is returned; if
    // it is positive, then length is set to {string_length} - {start}.

    CSA_ASSERT(this, IsHeapNumber(CAST(var_length.value())));

    Label if_isnegative(this), if_ispositive(this);
    TNode<Float64T> const float_zero = Float64Constant(0.);
    TNode<Float64T> const length_float =
        LoadHeapNumberValue(CAST(var_length.value()));
    Branch(Float64LessThan(length_float, float_zero), &if_isnegative,
           &if_ispositive);

    BIND(&if_isnegative);
    args.PopAndReturn(EmptyStringConstant());

    BIND(&if_ispositive);
    {
      var_result_length = IntPtrSub(string_length, var_start.value());
      GotoIfNot(IntPtrLessThanOrEqual(var_result_length.value(), zero), &out);
      args.PopAndReturn(EmptyStringConstant());
    }
  }

  BIND(&out);
  {
    TNode<IntPtrT> const end =
        IntPtrAdd(var_start.value(), var_result_length.value());
    args.PopAndReturn(SubString(string, var_start.value(), end));
  }
}

TNode<Smi> StringBuiltinsAssembler::ToSmiBetweenZeroAnd(
    SloppyTNode<Context> context, SloppyTNode<Object> value,
    SloppyTNode<Smi> limit) {
  Label out(this);
  TVARIABLE(Smi, var_result);

  TNode<Number> const value_int =
      ToInteger_Inline(context, value, CodeStubAssembler::kTruncateMinusZero);

  Label if_issmi(this), if_isnotsmi(this, Label::kDeferred);
  Branch(TaggedIsSmi(value_int), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    TNode<Smi> value_smi = CAST(value_int);
    Label if_isinbounds(this), if_isoutofbounds(this, Label::kDeferred);
    Branch(SmiAbove(value_smi, limit), &if_isoutofbounds, &if_isinbounds);

    BIND(&if_isinbounds);
    {
      var_result = CAST(value_int);
      Goto(&out);
    }

    BIND(&if_isoutofbounds);
    {
      TNode<Smi> const zero = SmiConstant(0);
      var_result =
          SelectConstant<Smi>(SmiLessThan(value_smi, zero), zero, limit);
      Goto(&out);
    }
  }

  BIND(&if_isnotsmi);
  {
    // {value} is a heap number - in this case, it is definitely out of bounds.
    TNode<HeapNumber> value_int_hn = CAST(value_int);

    TNode<Float64T> const float_zero = Float64Constant(0.);
    TNode<Smi> const smi_zero = SmiConstant(0);
    TNode<Float64T> const value_float = LoadHeapNumberValue(value_int_hn);
    var_result = SelectConstant<Smi>(Float64LessThan(value_float, float_zero),
                                     smi_zero, limit);
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

TF_BUILTIN(StringSubstring, CodeStubAssembler) {
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<IntPtrT> from = UncheckedCast<IntPtrT>(Parameter(Descriptor::kFrom));
  TNode<IntPtrT> to = UncheckedCast<IntPtrT>(Parameter(Descriptor::kTo));

  Return(SubString(string, from, to));
}

// ES6 #sec-string.prototype.substring
TF_BUILTIN(StringPrototypeSubstring, StringBuiltinsAssembler) {
  const int kStartArg = 0;
  const int kEndArg = 1;

  Node* const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* const receiver = args.GetReceiver();
  Node* const start = args.GetOptionalArgumentValue(kStartArg);
  Node* const end = args.GetOptionalArgumentValue(kEndArg);
  Node* const context = Parameter(Descriptor::kContext);

  Label out(this);

  TVARIABLE(Smi, var_start);
  TVARIABLE(Smi, var_end);

  // Check that {receiver} is coercible to Object and convert it to a String.
  TNode<String> const string =
      ToThisString(context, receiver, "String.prototype.substring");

  TNode<Smi> const length = LoadStringLengthAsSmi(string);

  // Conversion and bounds-checks for {start}.
  var_start = ToSmiBetweenZeroAnd(context, start, length);

  // Conversion and bounds-checks for {end}.
  {
    var_end = length;
    GotoIf(IsUndefined(end), &out);

    var_end = ToSmiBetweenZeroAnd(context, end, length);

    Label if_endislessthanstart(this);
    Branch(SmiLessThan(var_end.value(), var_start.value()),
           &if_endislessthanstart, &out);

    BIND(&if_endislessthanstart);
    {
      TNode<Smi> const tmp = var_end.value();
      var_end = var_start.value();
      var_start = tmp;
      Goto(&out);
    }
  }

  BIND(&out);
  {
    args.PopAndReturn(SubString(string, SmiUntag(var_start.value()),
                                SmiUntag(var_end.value())));
  }
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
  Node* const receiver = arguments.GetReceiver();

  // Check that {receiver} is coercible to Object and convert it to a String.
  TNode<String> const string = ToThisString(context, receiver, method_name);
  TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  Node* const string_data = to_direct.PointerToData(&if_runtime);
  Node* const instance_type = to_direct.instance_type();
  Node* const is_stringonebyte = IsOneByteStringInstanceType(instance_type);
  Node* const string_data_offset = to_direct.offset();

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
    Node* const string_data, Node* const string_data_offset,
    Node* const is_stringonebyte, Variable* const var_index, Node* const end,
    int increment, Label* const if_none_found) {
  Label if_stringisonebyte(this), out(this);

  GotoIf(is_stringonebyte, &if_stringisonebyte);

  // Two Byte String
  BuildLoop(
      var_index, end, increment, if_none_found, &out, [&](Node* const index) {
        return Load(
            MachineType::Uint16(), string_data,
            WordShl(IntPtrAdd(index, string_data_offset), IntPtrConstant(1)));
      });

  BIND(&if_stringisonebyte);
  BuildLoop(var_index, end, increment, if_none_found, &out,
            [&](Node* const index) {
              return Load(MachineType::Uint8(), string_data,
                          IntPtrAdd(index, string_data_offset));
            });

  BIND(&out);
}

void StringTrimAssembler::BuildLoop(Variable* const var_index, Node* const end,
                                    int increment, Label* const if_none_found,
                                    Label* const out,
                                    std::function<Node*(Node*)> get_character) {
  Label loop(this, var_index);
  Goto(&loop);
  BIND(&loop);
  {
    Node* const index = var_index->value();
    GotoIf(IntPtrEqual(index, end), if_none_found);
    GotoIfNotWhiteSpaceOrLineTerminator(
        UncheckedCast<Uint32T>(get_character(index)), out);
    Increment(var_index, increment);
    Goto(&loop);
  }
}

void StringTrimAssembler::GotoIfNotWhiteSpaceOrLineTerminator(
    Node* const char_code, Label* const if_not_whitespace) {
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

// ES6 #sec-string.prototype.tostring
TF_BUILTIN(StringPrototypeToString, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kString,
                             "String.prototype.toString");
  Return(result);
}

// ES6 #sec-string.prototype.valueof
TF_BUILTIN(StringPrototypeValueOf, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kString,
                             "String.prototype.valueOf");
  Return(result);
}

TF_BUILTIN(StringPrototypeIterator, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* string =
      ToThisString(context, receiver, "String.prototype[Symbol.iterator]");

  Node* native_context = LoadNativeContext(context);
  Node* map =
      LoadContextElement(native_context, Context::STRING_ITERATOR_MAP_INDEX);
  Node* iterator = Allocate(JSStringIterator::kSize);
  StoreMapNoWriteBarrier(iterator, map);
  StoreObjectFieldRoot(iterator, JSValue::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(iterator, JSObject::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kStringOffset,
                                 string);
  Node* index = SmiConstant(0);
  StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kNextIndexOffset,
                                 index);
  Return(iterator);
}

// Return the |word32| codepoint at {index}. Supports SeqStrings and
// ExternalStrings.
TNode<Int32T> StringBuiltinsAssembler::LoadSurrogatePairAt(
    SloppyTNode<String> string, SloppyTNode<IntPtrT> length,
    SloppyTNode<IntPtrT> index, UnicodeEncoding encoding) {
  Label handle_surrogate_pair(this), return_result(this);
  TVARIABLE(Int32T, var_result);
  TVARIABLE(Int32T, var_trail);
  var_result = StringCharCodeAt(string, index);
  var_trail = Int32Constant(0);

  GotoIf(Word32NotEqual(Word32And(var_result.value(), Int32Constant(0xFC00)),
                        Int32Constant(0xD800)),
         &return_result);
  TNode<IntPtrT> next_index = IntPtrAdd(index, IntPtrConstant(1));

  GotoIfNot(IntPtrLessThan(next_index, length), &return_result);
  var_trail = StringCharCodeAt(string, next_index);
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
        var_result = Signed(Word32Or(
// Need to swap the order for big-endian platforms
#if V8_TARGET_BIG_ENDIAN
            Word32Shl(lead, Int32Constant(16)), trail));
#else
            Word32Shl(trail, Int32Constant(16)), lead));
#endif
        break;

      case UnicodeEncoding::UTF32: {
        // Convert UTF16 surrogate pair into |word32| code point, encoded as
        // UTF32.
        TNode<Int32T> surrogate_offset =
            Int32Constant(0x10000 - (0xD800 << 10) - 0xDC00);

        // (lead << 10) + trail + SURROGATE_OFFSET
        var_result = Signed(Int32Add(Word32Shl(lead, Int32Constant(10)),
                                     Int32Add(trail, surrogate_offset)));
        break;
      }
    }
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

// ES6 #sec-%stringiteratorprototype%.next
TF_BUILTIN(StringIteratorPrototypeNext, StringBuiltinsAssembler) {
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_done, MachineRepresentation::kTagged);

  var_value.Bind(UndefinedConstant());
  var_done.Bind(TrueConstant());

  Label throw_bad_receiver(this), next_codepoint(this), return_result(this);

  Node* context = Parameter(Descriptor::kContext);
  Node* iterator = Parameter(Descriptor::kReceiver);

  GotoIf(TaggedIsSmi(iterator), &throw_bad_receiver);
  GotoIfNot(
      InstanceTypeEqual(LoadInstanceType(iterator), JS_STRING_ITERATOR_TYPE),
      &throw_bad_receiver);

  Node* string = LoadObjectField(iterator, JSStringIterator::kStringOffset);
  TNode<IntPtrT> position = SmiUntag(
      CAST(LoadObjectField(iterator, JSStringIterator::kNextIndexOffset)));
  TNode<IntPtrT> length = LoadStringLengthAsWord(string);

  Branch(IntPtrLessThan(position, length), &next_codepoint, &return_result);

  BIND(&next_codepoint);
  {
    UnicodeEncoding encoding = UnicodeEncoding::UTF16;
    TNode<Int32T> ch = LoadSurrogatePairAt(string, length, position, encoding);
    TNode<String> value = StringFromSingleCodePoint(ch, encoding);
    var_value.Bind(value);
    TNode<IntPtrT> length = LoadStringLengthAsWord(value);
    StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kNextIndexOffset,
                                   SmiTag(Signed(IntPtrAdd(position, length))));
    var_done.Bind(FalseConstant());
    Goto(&return_result);
  }

  BIND(&return_result);
  {
    Node* result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }

  BIND(&throw_bad_receiver);
  {
    // The {receiver} is not a valid JSGeneratorObject.
    ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                   StringConstant("String Iterator.prototype.next"), iterator);
  }
}

// -----------------------------------------------------------------------------
// ES6 section B.2.3 Additional Properties of the String.prototype object

class StringHtmlAssembler : public StringBuiltinsAssembler {
 public:
  explicit StringHtmlAssembler(compiler::CodeAssemblerState* state)
      : StringBuiltinsAssembler(state) {}

 protected:
  void Generate(Node* const context, Node* const receiver,
                const char* method_name, const char* tag_name) {
    Node* const string = ToThisString(context, receiver, method_name);
    std::string open_tag = "<" + std::string(tag_name) + ">";
    std::string close_tag = "</" + std::string(tag_name) + ">";

    Node* strings[] = {StringConstant(open_tag.c_str()), string,
                       StringConstant(close_tag.c_str())};
    Return(ConcatStrings(context, strings, arraysize(strings)));
  }

  void GenerateWithAttribute(Node* const context, Node* const receiver,
                             const char* method_name, const char* tag_name,
                             const char* attr, Node* const value) {
    Node* const string = ToThisString(context, receiver, method_name);
    Node* const value_string =
        EscapeQuotes(context, ToString_Inline(context, value));
    std::string open_tag_attr =
        "<" + std::string(tag_name) + " " + std::string(attr) + "=\"";
    std::string close_tag = "</" + std::string(tag_name) + ">";

    Node* strings[] = {StringConstant(open_tag_attr.c_str()), value_string,
                       StringConstant("\">"), string,
                       StringConstant(close_tag.c_str())};
    Return(ConcatStrings(context, strings, arraysize(strings)));
  }

  Node* ConcatStrings(Node* const context, Node** strings, int len) {
    VARIABLE(var_result, MachineRepresentation::kTagged, strings[0]);
    for (int i = 1; i < len; i++) {
      var_result.Bind(CallStub(CodeFactory::StringAdd(isolate()), context,
                               var_result.value(), strings[i]));
    }
    return var_result.value();
  }

  Node* EscapeQuotes(Node* const context, Node* const string) {
    CSA_ASSERT(this, IsString(string));
    Node* const regexp_function = LoadContextElement(
        LoadNativeContext(context), Context::REGEXP_FUNCTION_INDEX);
    Node* const initial_map = LoadObjectField(
        regexp_function, JSFunction::kPrototypeOrInitialMapOffset);
    // TODO(pwong): Refactor to not allocate RegExp
    Node* const regexp =
        CallRuntime(Runtime::kRegExpInitializeAndCompile, context,
                    AllocateJSObjectFromMap(initial_map), StringConstant("\""),
                    StringConstant("g"));

    return CallRuntime(Runtime::kRegExpInternalReplace, context, regexp, string,
                       StringConstant("&quot;"));
  }
};

// ES6 #sec-string.prototype.anchor
TF_BUILTIN(StringPrototypeAnchor, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const value = Parameter(Descriptor::kValue);
  GenerateWithAttribute(context, receiver, "String.prototype.anchor", "a",
                        "name", value);
}

// ES6 #sec-string.prototype.big
TF_BUILTIN(StringPrototypeBig, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.big", "big");
}

// ES6 #sec-string.prototype.blink
TF_BUILTIN(StringPrototypeBlink, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.blink", "blink");
}

// ES6 #sec-string.prototype.bold
TF_BUILTIN(StringPrototypeBold, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.bold", "b");
}

// ES6 #sec-string.prototype.fontcolor
TF_BUILTIN(StringPrototypeFontcolor, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const value = Parameter(Descriptor::kValue);
  GenerateWithAttribute(context, receiver, "String.prototype.fontcolor", "font",
                        "color", value);
}

// ES6 #sec-string.prototype.fontsize
TF_BUILTIN(StringPrototypeFontsize, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const value = Parameter(Descriptor::kValue);
  GenerateWithAttribute(context, receiver, "String.prototype.fontsize", "font",
                        "size", value);
}

// ES6 #sec-string.prototype.fixed
TF_BUILTIN(StringPrototypeFixed, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.fixed", "tt");
}

// ES6 #sec-string.prototype.italics
TF_BUILTIN(StringPrototypeItalics, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.italics", "i");
}

// ES6 #sec-string.prototype.link
TF_BUILTIN(StringPrototypeLink, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const value = Parameter(Descriptor::kValue);
  GenerateWithAttribute(context, receiver, "String.prototype.link", "a", "href",
                        value);
}

// ES6 #sec-string.prototype.small
TF_BUILTIN(StringPrototypeSmall, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.small", "small");
}

// ES6 #sec-string.prototype.strike
TF_BUILTIN(StringPrototypeStrike, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.strike", "strike");
}

// ES6 #sec-string.prototype.sub
TF_BUILTIN(StringPrototypeSub, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.sub", "sub");
}

// ES6 #sec-string.prototype.sup
TF_BUILTIN(StringPrototypeSup, StringHtmlAssembler) {
  Node* const context = Parameter(Descriptor::kContext);
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Generate(context, receiver, "String.prototype.sup", "sup");
}

}  // namespace internal
}  // namespace v8
