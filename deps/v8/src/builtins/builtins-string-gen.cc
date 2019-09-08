// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-string-gen.h"

#include "src/builtins/builtins-regexp-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/objects.h"
#include "src/objects/property-cell.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;
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
    // pointer is cached (i.e. no uncached external strings).
    CSA_ASSERT(this, Word32NotEqual(
                         Word32And(string_instance_type,
                                   Int32Constant(kUncachedExternalStringMask)),
                         Int32Constant(kUncachedExternalStringTag)));
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

  TNode<Int32T> const encoding_mask = Int32Constant(kStringEncodingMask);
  TNode<Word32T> const lhs_encoding =
      Word32And(lhs_instance_type, encoding_mask);
  TNode<Word32T> const rhs_encoding =
      Word32And(rhs_instance_type, encoding_mask);

  TNode<Word32T> const combined_encodings =
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
  TNode<ExternalReference> const function_addr = ExternalConstant(
      ExternalReference::search_string_raw<SubjectChar, PatternChar>());
  TNode<ExternalReference> const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_intptr = MachineType::IntPtr();

  Node* const result = CallCFunction(
      function_addr, type_intptr, std::make_pair(type_ptr, isolate_ptr),
      std::make_pair(type_ptr, subject_ptr),
      std::make_pair(type_intptr, subject_length),
      std::make_pair(type_ptr, search_ptr),
      std::make_pair(type_intptr, search_length),
      std::make_pair(type_intptr, start_position));

  return result;
}

TNode<IntPtrT> StringBuiltinsAssembler::PointerToStringDataAtIndex(
    Node* const string_data, Node* const index, String::Encoding encoding) {
  const ElementsKind kind = (encoding == String::ONE_BYTE_ENCODING)
                                ? UINT8_ELEMENTS
                                : UINT16_ELEMENTS;
  TNode<IntPtrT> const offset_in_bytes =
      ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS);
  return Signed(IntPtrAdd(string_data, offset_in_bytes));
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
    SloppyTNode<String> lhs, Node* lhs_instance_type, SloppyTNode<String> rhs,
    Node* rhs_instance_type, TNode<IntPtrT> length, Label* if_equal,
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

  Variable* input_vars[2] = {&var_left, &var_right};
  Label if_less(this), if_equal(this), if_greater(this);
  Label restart(this, 2, input_vars);
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

TF_BUILTIN(StringCharAt, StringBuiltinsAssembler) {
  TNode<String> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<IntPtrT> position =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kPosition));

  // Load the character code at the {position} from the {receiver}.
  TNode<Int32T> code = StringCharCodeAt(receiver, position);

  // And return the single character string with only that {code}
  TNode<String> result = StringFromSingleCharCode(code);
  Return(result);
}

TF_BUILTIN(StringCodePointAt, StringBuiltinsAssembler) {
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
TF_BUILTIN(StringFromCharCode, CodeStubAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  Node* context = Parameter(Descriptor::kContext);

  CodeStubArguments arguments(this, ChangeInt32ToIntPtr(argc));
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
    Node* code32 = TruncateTaggedToWord32(context, code);
    TNode<Int32T> code16 =
        Signed(Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit)));
    TNode<String> result = StringFromSingleCharCode(code16);
    arguments.PopAndReturn(result);
  }

  Node* code16 = nullptr;
  BIND(&if_notoneargument);
  {
    Label two_byte(this);
    // Assume that the resulting string contains only one-byte characters.
    TNode<String> one_byte_result = AllocateSeqOneByteString(Unsigned(argc));

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
      TNode<IntPtrT> offset = ElementOffsetFromIndex(
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
          TNode<Word32T> code16 =
              Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

          TNode<IntPtrT> offset = ElementOffsetFromIndex(
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

void StringBuiltinsAssembler::StringIndexOf(
    TNode<String> const subject_string, TNode<String> const search_string,
    TNode<Smi> const position,
    const std::function<void(TNode<Smi>)>& f_return) {
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
  GotoIf(TaggedEqual(subject_string, search_string), &return_zero);

  // Try to unpack subject and search strings. Bail to runtime if either needs
  // to be flattened.
  ToDirectStringAssembler subject_to_direct(state(), subject_string);
  ToDirectStringAssembler search_to_direct(state(), search_string);

  Label call_runtime_unchecked(this, Label::kDeferred);

  subject_to_direct.TryToDirect(&call_runtime_unchecked);
  search_to_direct.TryToDirect(&call_runtime_unchecked);

  // Load pointers to string data.
  TNode<RawPtrT> const subject_ptr =
      subject_to_direct.PointerToData(&call_runtime_unchecked);
  TNode<RawPtrT> const search_ptr =
      search_to_direct.PointerToData(&call_runtime_unchecked);

  TNode<IntPtrT> const subject_offset = subject_to_direct.offset();
  TNode<IntPtrT> const search_offset = search_to_direct.offset();

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
    TNode<IntPtrT> const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    TNode<IntPtrT> const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    Label direct_memchr_call(this), generic_fast_path(this);
    Branch(IntPtrEqual(search_length, IntPtrConstant(1)), &direct_memchr_call,
           &generic_fast_path);

    // An additional fast path that calls directly into memchr for 1-length
    // search strings.
    BIND(&direct_memchr_call);
    {
      TNode<IntPtrT> const string_addr =
          IntPtrAdd(adjusted_subject_ptr, start_position);
      TNode<IntPtrT> const search_length =
          IntPtrSub(subject_length, start_position);
      TNode<IntPtrT> const search_byte =
          ChangeInt32ToIntPtr(Load(MachineType::Uint8(), adjusted_search_ptr));

      TNode<ExternalReference> const memchr =
          ExternalConstant(ExternalReference::libc_memchr_function());
      TNode<IntPtrT> const result_address = UncheckedCast<IntPtrT>(
          CallCFunction(memchr, MachineType::Pointer(),
                        std::make_pair(MachineType::Pointer(), string_addr),
                        std::make_pair(MachineType::IntPtr(), search_byte),
                        std::make_pair(MachineType::UintPtr(), search_length)));
      GotoIf(WordEqual(result_address, int_zero), &return_minus_1);
      TNode<IntPtrT> const result_index =
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
    TNode<IntPtrT> const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::ONE_BYTE_ENCODING);
    TNode<IntPtrT> const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::TWO_BYTE_ENCODING);

    Node* const result = CallSearchStringRaw<onebyte_t, twobyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_one);
  {
    TNode<IntPtrT> const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    TNode<IntPtrT> const adjusted_search_ptr = PointerToStringDataAtIndex(
        search_ptr, search_offset, String::ONE_BYTE_ENCODING);

    Node* const result = CallSearchStringRaw<twobyte_t, onebyte_t>(
        adjusted_subject_ptr, subject_length, adjusted_search_ptr,
        search_length, start_position);
    f_return(SmiTag(result));
  }

  BIND(&two_two);
  {
    TNode<IntPtrT> const adjusted_subject_ptr = PointerToStringDataAtIndex(
        subject_ptr, subject_offset, String::TWO_BYTE_ENCODING);
    TNode<IntPtrT> const adjusted_search_ptr = PointerToStringDataAtIndex(
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
  TNode<Object> const receiver = arguments.GetReceiver();

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
    TNode<Object> const search = var_search_string.value();
    TNode<Smi> const position = CAST(var_position.value());
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
    TNode<Object> const result =
        CallRuntime(runtime, context, receiver, var_search_string.value(),
                    var_position.value());
    arguments.PopAndReturn(result);
  }
}

void StringBuiltinsAssembler::MaybeCallFunctionAtSymbol(
    Node* const context, Node* const object, Node* const maybe_string,
    Handle<Symbol> symbol,
    DescriptorIndexNameValue additional_property_to_check,
    const NodeFunction0& regexp_call, const NodeFunction1& generic_call) {
  Label out(this);

  // Smis definitely don't have an attached symbol.
  GotoIf(TaggedIsSmi(object), &out);

  // Take the fast path for RegExps.
  // There's two conditions: {object} needs to be a fast regexp, and
  // {maybe_string} must be a string (we can't call ToString on the fast path
  // since it may mutate {object}).
  {
    Label stub_call(this), slow_lookup(this);

    GotoIf(TaggedIsSmi(maybe_string), &slow_lookup);
    GotoIfNot(IsString(maybe_string), &slow_lookup);

    // Note we don't run a full (= permissive) check here, because passing the
    // check implies calling the fast variants of target builtins, which assume
    // we've already made their appropriate fast path checks. This is not the
    // case though; e.g.: some of the target builtins access flag getters.
    // TODO(jgruber): Handle slow flag accesses on the fast path and make this
    // permissive.
    RegExpBuiltinsAssembler regexp_asm(state());
    regexp_asm.BranchIfFastRegExp(
        CAST(context), CAST(object), LoadMap(object),
        PrototypeCheckAssembler::kCheckPrototypePropertyConstness,
        additional_property_to_check, &stub_call, &slow_lookup);

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

  TNode<Object> const maybe_func = GetProperty(context, object, symbol);
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

    TNode<Object> const matched =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_start_index), SmiUntag(match_end_index));
    TNode<Object> const replacement_string =
        CallRuntime(Runtime::kGetSubstitution, context, matched, subject_string,
                    match_start_index, replace_string, dollar_index);
    var_result.Bind(replacement_string);

    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES6 #sec-string.prototype.replace
TF_BUILTIN(StringPrototypeReplace, StringBuiltinsAssembler) {
  Label out(this);

  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Node* const search = Parameter(Descriptor::kSearch);
  Node* const replace = Parameter(Descriptor::kReplace);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TNode<Smi> const smi_zero = SmiConstant(0);

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
      [=](Node* fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        Return(CallJS(call_callable, context, fn, search, receiver, replace));
      });

  // Convert {receiver} and {search} to strings.

  TNode<String> const subject_string = ToString_Inline(context, receiver);
  TNode<String> const search_string = ToString_Inline(context, search);

  TNode<IntPtrT> const subject_length = LoadStringLengthAsWord(subject_string);
  TNode<IntPtrT> const search_length = LoadStringLengthAsWord(search_string);

  // Fast-path single-char {search}, long cons {receiver}, and simple string
  // {replace}.
  {
    Label next(this);

    GotoIfNot(WordEqual(search_length, IntPtrConstant(1)), &next);
    GotoIfNot(IntPtrGreaterThan(subject_length, IntPtrConstant(0xFF)), &next);
    GotoIf(TaggedIsSmi(replace), &next);
    GotoIfNot(IsString(replace), &next);

    TNode<Uint16T> const subject_instance_type =
        LoadInstanceType(subject_string);
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

  TNode<Smi> const match_end_index =
      SmiAdd(match_start_index, SmiFromIntPtr(search_length));

  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  // Compute the prefix.
  {
    Label next(this);

    GotoIf(SmiEqual(match_start_index, smi_zero), &next);
    TNode<Object> const prefix =
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
    TNode<String> const replacement_string =
        ToString_Inline(context, replacement);
    var_result.Bind(CallBuiltin(Builtins::kStringAdd_CheckNone, context,
                                var_result.value(), replacement_string));
    Goto(&out);
  }

  BIND(&if_notcallablereplace);
  {
    TNode<String> const replace_string = ToString_Inline(context, replace);
    Node* const replacement =
        GetSubstitution(context, subject_string, match_start_index,
                        match_end_index, replace_string);
    var_result.Bind(CallBuiltin(Builtins::kStringAdd_CheckNone, context,
                                var_result.value(), replacement));
    Goto(&out);
  }

  BIND(&out);
  {
    TNode<Object> const suffix =
        CallBuiltin(Builtins::kStringSubstring, context, subject_string,
                    SmiUntag(match_end_index), subject_length);
    TNode<Object> const result = CallBuiltin(
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
        [=](Node* fn) {
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

  // 2. If regexp is neither undefined nor null, then
  //   a. Let matcher be ? GetMethod(regexp, @@matchAll).
  //   b. If matcher is not undefined, then
  //     i. Return ? Call(matcher, regexp, « O »).
  auto if_regexp_call = [&] {
    // MaybeCallFunctionAtSymbol guarantees fast path is chosen only if
    // maybe_regexp is a fast regexp and receiver is a string.
    TNode<String> s = CAST(receiver);

    RegExpMatchAllAssembler regexp_asm(state());
    regexp_asm.Generate(context, native_context, maybe_regexp, s);
  };
  auto if_generic_call = [=](Node* fn) {
    Callable call_callable = CodeFactory::Call(isolate());
    Return(CallJS(call_callable, context, fn, maybe_regexp, receiver));
  };
  MaybeCallFunctionAtSymbol(
      context, maybe_regexp, receiver, isolate()->factory()->match_all_symbol(),
      DescriptorIndexNameValue{JSRegExp::kSymbolMatchAllFunctionDescriptorIndex,
                               RootIndex::kmatch_all_symbol,
                               Context::REGEXP_MATCH_ALL_FUNCTION_INDEX},
      if_regexp_call, if_generic_call);

  RegExpMatchAllAssembler regexp_asm(state());

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
          TNode<UintPtrT> code_index = ChangeUint32ToWord(char_code);
          TNode<Object> entry = LoadFixedArrayElement(cache, code_index);

          // If we cannot find a char in the cache, fill the hole for the fixed
          // array, and call runtime.
          GotoIf(IsUndefined(entry), &fill_thehole_and_call_runtime);

          StoreFixedArrayElement(elements, index, entry);
        },
        1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

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

  TNode<IntPtrT> const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  TNode<Object> receiver = args.GetReceiver();
  TNode<Object> const separator = args.GetOptionalArgumentValue(kSeparatorArg);
  TNode<Object> const limit = args.GetOptionalArgumentValue(kLimitArg);
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
  TNode<String> const separator_string = ToString_Inline(context, separator);

  Label return_empty_array(this);

  // Shortcut for {limit} == 0.
  GotoIf(TaggedEqual(limit_number, smi_zero), &return_empty_array);

  // ECMA-262 says that if {separator} is undefined, the result should
  // be an array of size 1 containing the entire string.
  {
    Label next(this);
    GotoIfNot(IsUndefined(separator), &next);

    const ElementsKind kind = PACKED_ELEMENTS;
    TNode<NativeContext> const native_context = LoadNativeContext(context);
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

  TNode<Object> const result =
      CallRuntime(Runtime::kStringSplit, context, subject_string,
                  separator_string, limit_number);
  args.PopAndReturn(result);

  BIND(&return_empty_array);
  {
    const ElementsKind kind = PACKED_ELEMENTS;
    TNode<NativeContext> const native_context = LoadNativeContext(context);
    TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);

    TNode<Smi> length = smi_zero;
    TNode<IntPtrT> capacity = IntPtrConstant(0);
    TNode<JSArray> result = AllocateJSArray(kind, array_map, capacity, length);

    args.PopAndReturn(result);
  }
}

// ES6 #sec-string.prototype.substr
TF_BUILTIN(StringPrototypeSubstr, StringBuiltinsAssembler) {
  const int kStartArg = 0;
  const int kLengthArg = 1;

  TNode<IntPtrT> const argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  TNode<Object> receiver = args.GetReceiver();
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

TF_BUILTIN(StringSubstring, CodeStubAssembler) {
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
  TNode<String> const string = ToThisString(context, receiver, method_name);
  TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  TNode<RawPtrT> const string_data = to_direct.PointerToData(&if_runtime);
  TNode<Int32T> const instance_type = to_direct.instance_type();
  TNode<BoolT> const is_stringonebyte =
      IsOneByteStringInstanceType(instance_type);
  TNode<IntPtrT> const string_data_offset = to_direct.offset();

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
    Node* const is_stringonebyte, TVariable<IntPtrT>* const var_index,
    TNode<IntPtrT> const end, int increment, Label* const if_none_found) {
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

void StringTrimAssembler::BuildLoop(
    TVariable<IntPtrT>* const var_index, TNode<IntPtrT> const end,
    int increment, Label* const if_none_found, Label* const out,
    const std::function<Node*(Node*)>& get_character) {
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
    TNode<Word32T> const char_code, Label* const if_not_whitespace) {
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
                  SmiConstant(Isolate::kProtectorValid)),
      if_true, if_false);
}

}  // namespace internal
}  // namespace v8
