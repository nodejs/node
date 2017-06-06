// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-regexp-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

typedef CodeStubAssembler::RelationalComparisonMode RelationalComparisonMode;

class StringBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit StringBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  Node* DirectStringData(Node* string, Node* string_instance_type) {
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
      CSA_ASSERT(this, Word32NotEqual(
                           Word32And(string_instance_type,
                                     Int32Constant(kShortExternalStringMask)),
                           Int32Constant(kShortExternalStringTag)));
      var_data.Bind(LoadObjectField(string, ExternalString::kResourceDataOffset,
                                    MachineType::Pointer()));
      Goto(&if_join);
    }

    BIND(&if_join);
    return var_data.value();
  }

  Node* LoadOneByteChar(Node* string, Node* index) {
    return Load(MachineType::Uint8(), string, OneByteCharOffset(index));
  }

  Node* OneByteCharAddress(Node* string, Node* index) {
    Node* offset = OneByteCharOffset(index);
    return IntPtrAdd(string, offset);
  }

  Node* OneByteCharOffset(Node* index) {
    return CharOffset(String::ONE_BYTE_ENCODING, index);
  }

  Node* CharOffset(String::Encoding encoding, Node* index) {
    const int header = SeqOneByteString::kHeaderSize - kHeapObjectTag;
    Node* offset = index;
    if (encoding == String::TWO_BYTE_ENCODING) {
      offset = IntPtrAdd(offset, offset);
    }
    offset = IntPtrAdd(offset, IntPtrConstant(header));
    return offset;
  }

  void DispatchOnStringInstanceType(Node* const instance_type,
                                    Label* if_onebyte_sequential,
                                    Label* if_onebyte_external,
                                    Label* if_otherwise) {
    const int kMask = kStringRepresentationMask | kStringEncodingMask;
    Node* const encoding_and_representation =
        Word32And(instance_type, Int32Constant(kMask));

    int32_t values[] = {
        kOneByteStringTag | kSeqStringTag,
        kOneByteStringTag | kExternalStringTag,
    };
    Label* labels[] = {
        if_onebyte_sequential, if_onebyte_external,
    };
    STATIC_ASSERT(arraysize(values) == arraysize(labels));

    Switch(encoding_and_representation, if_otherwise, values, labels,
           arraysize(values));
  }

  void GenerateStringEqual(Node* context, Node* left, Node* right);
  void GenerateStringRelationalComparison(Node* context, Node* left,
                                          Node* right,
                                          RelationalComparisonMode mode);

  Node* ToSmiBetweenZeroAnd(Node* context, Node* value, Node* limit);

  Node* LoadSurrogatePairAt(Node* string, Node* length, Node* index,
                            UnicodeEncoding encoding);

  void StringIndexOf(Node* receiver, Node* instance_type, Node* search_string,
                     Node* search_string_instance_type, Node* position,
                     std::function<void(Node*)> f_return);

  Node* IsNullOrUndefined(Node* const value);
  void RequireObjectCoercible(Node* const context, Node* const value,
                              const char* method_name);

  Node* SmiIsNegative(Node* const value) {
    return SmiLessThan(value, SmiConstant(0));
  }

  // Implements boilerplate logic for {match, split, replace, search} of the
  // form:
  //
  //  if (!IS_NULL_OR_UNDEFINED(object)) {
  //    var maybe_function = object[symbol];
  //    if (!IS_UNDEFINED(maybe_function)) {
  //      return %_Call(maybe_function, ...);
  //    }
  //  }
  //
  // Contains fast paths for Smi and RegExp objects.
  typedef std::function<Node*()> NodeFunction0;
  typedef std::function<Node*(Node* fn)> NodeFunction1;
  void MaybeCallFunctionAtSymbol(Node* const context, Node* const object,
                                 Handle<Symbol> symbol,
                                 const NodeFunction0& regexp_call,
                                 const NodeFunction1& generic_call);
};

void StringBuiltinsAssembler::GenerateStringEqual(Node* context, Node* left,
                                                  Node* right) {
  // Here's pseudo-code for the algorithm below:
  //
  // if (lhs == rhs) return true;
  // if (lhs->length() != rhs->length()) return false;
  // if (lhs->IsInternalizedString() && rhs->IsInternalizedString()) {
  //   return false;
  // }
  // if (lhs->IsSeqOneByteString() && rhs->IsSeqOneByteString()) {
  //   for (i = 0; i != lhs->length(); ++i) {
  //     if (lhs[i] != rhs[i]) return false;
  //   }
  //   return true;
  // }
  // if (lhs and/or rhs are indirect strings) {
  //   unwrap them and restart from the beginning;
  // }
  // return %StringEqual(lhs, rhs);

  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);

  Variable* input_vars[2] = {&var_left, &var_right};
  Label if_equal(this), if_notequal(this), restart(this, 2, input_vars);
  Goto(&restart);
  BIND(&restart);
  Node* lhs = var_left.value();
  Node* rhs = var_right.value();

  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  GotoIf(WordEqual(lhs, rhs), &if_equal);

  // Load the length of {lhs} and {rhs}.
  Node* lhs_length = LoadStringLength(lhs);
  Node* rhs_length = LoadStringLength(rhs);

  // Strings with different lengths cannot be equal.
  GotoIf(WordNotEqual(lhs_length, rhs_length), &if_notequal);

  // Load instance types of {lhs} and {rhs}.
  Node* lhs_instance_type = LoadInstanceType(lhs);
  Node* rhs_instance_type = LoadInstanceType(rhs);

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
         &if_notequal);

  // Check that both {lhs} and {rhs} are flat one-byte strings, and that
  // in case of ExternalStrings the data pointer is cached..
  STATIC_ASSERT(kShortExternalStringTag != 0);
  int const kBothDirectOneByteStringMask =
      kStringEncodingMask | kIsIndirectStringMask | kShortExternalStringMask |
      ((kStringEncodingMask | kIsIndirectStringMask | kShortExternalStringMask)
       << 8);
  int const kBothDirectOneByteStringTag =
      kOneByteStringTag | (kOneByteStringTag << 8);
  Label if_bothdirectonebytestrings(this), if_notbothdirectonebytestrings(this);
  Branch(Word32Equal(Word32And(both_instance_types,
                               Int32Constant(kBothDirectOneByteStringMask)),
                     Int32Constant(kBothDirectOneByteStringTag)),
         &if_bothdirectonebytestrings, &if_notbothdirectonebytestrings);

  BIND(&if_bothdirectonebytestrings);
  {
    // Compute the effective offset of the first character.
    Node* lhs_data = DirectStringData(lhs, lhs_instance_type);
    Node* rhs_data = DirectStringData(rhs, rhs_instance_type);

    // Compute the first offset after the string from the length.
    Node* length = SmiUntag(lhs_length);

    // Loop over the {lhs} and {rhs} strings to see if they are equal.
    VARIABLE(var_offset, MachineType::PointerRepresentation());
    Label loop(this, &var_offset);
    var_offset.Bind(IntPtrConstant(0));
    Goto(&loop);
    BIND(&loop);
    {
      // If {offset} equals {end}, no difference was found, so the
      // strings are equal.
      Node* offset = var_offset.value();
      GotoIf(WordEqual(offset, length), &if_equal);

      // Load the next characters from {lhs} and {rhs}.
      Node* lhs_value = Load(MachineType::Uint8(), lhs_data, offset);
      Node* rhs_value = Load(MachineType::Uint8(), rhs_data, offset);

      // Check if the characters match.
      GotoIf(Word32NotEqual(lhs_value, rhs_value), &if_notequal);

      // Advance to next character.
      var_offset.Bind(IntPtrAdd(offset, IntPtrConstant(1)));
      Goto(&loop);
    }
  }

  BIND(&if_notbothdirectonebytestrings);
  {
    // Try to unwrap indirect strings, restart the above attempt on success.
    MaybeDerefIndirectStrings(&var_left, lhs_instance_type, &var_right,
                              rhs_instance_type, &restart);
    // TODO(bmeurer): Add support for two byte string equality checks.

    TailCallRuntime(Runtime::kStringEqual, context, lhs, rhs);
  }

  BIND(&if_equal);
  Return(TrueConstant());

  BIND(&if_notequal);
  Return(FalseConstant());
}

void StringBuiltinsAssembler::GenerateStringRelationalComparison(
    Node* context, Node* left, Node* right, RelationalComparisonMode mode) {
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
    Node* lhs_length = LoadStringLength(lhs);
    Node* rhs_length = LoadStringLength(rhs);

    // Determine the minimum length.
    Node* length = SmiMin(lhs_length, rhs_length);

    // Compute the effective offset of the first character.
    Node* begin =
        IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag);

    // Compute the first offset after the string from the length.
    Node* end = IntPtrAdd(begin, SmiUntag(length));

    // Loop over the {lhs} and {rhs} strings to see if they are equal.
    VARIABLE(var_offset, MachineType::PointerRepresentation());
    Label loop(this, &var_offset);
    var_offset.Bind(begin);
    Goto(&loop);
    BIND(&loop);
    {
      // Check if {offset} equals {end}.
      Node* offset = var_offset.value();
      Label if_done(this), if_notdone(this);
      Branch(WordEqual(offset, end), &if_done, &if_notdone);

      BIND(&if_notdone);
      {
        // Load the next characters from {lhs} and {rhs}.
        Node* lhs_value = Load(MachineType::Uint8(), lhs, offset);
        Node* rhs_value = Load(MachineType::Uint8(), rhs, offset);

        // Check if the characters match.
        Label if_valueissame(this), if_valueisnotsame(this);
        Branch(Word32Equal(lhs_value, rhs_value), &if_valueissame,
               &if_valueisnotsame);

        BIND(&if_valueissame);
        {
          // Advance to next character.
          var_offset.Bind(IntPtrAdd(offset, IntPtrConstant(1)));
        }
        Goto(&loop);

        BIND(&if_valueisnotsame);
        Branch(Uint32LessThan(lhs_value, rhs_value), &if_less, &if_greater);
      }

      BIND(&if_done);
      {
        // All characters up to the min length are equal, decide based on
        // string length.
        GotoIf(SmiEqual(lhs_length, rhs_length), &if_equal);
        BranchIfSmiLessThan(lhs_length, rhs_length, &if_less, &if_greater);
      }
    }
  }

  BIND(&if_notbothonebyteseqstrings);
  {
    // Try to unwrap indirect strings, restart the above attempt on success.
    MaybeDerefIndirectStrings(&var_left, lhs_instance_type, &var_right,
                              rhs_instance_type, &restart);
    // TODO(bmeurer): Add support for two byte string relational comparisons.
    switch (mode) {
      case RelationalComparisonMode::kLessThan:
        TailCallRuntime(Runtime::kStringLessThan, context, lhs, rhs);
        break;
      case RelationalComparisonMode::kLessThanOrEqual:
        TailCallRuntime(Runtime::kStringLessThanOrEqual, context, lhs, rhs);
        break;
      case RelationalComparisonMode::kGreaterThan:
        TailCallRuntime(Runtime::kStringGreaterThan, context, lhs, rhs);
        break;
      case RelationalComparisonMode::kGreaterThanOrEqual:
        TailCallRuntime(Runtime::kStringGreaterThanOrEqual, context, lhs, rhs);
        break;
    }
  }

  BIND(&if_less);
  switch (mode) {
    case RelationalComparisonMode::kLessThan:
    case RelationalComparisonMode::kLessThanOrEqual:
      Return(BooleanConstant(true));
      break;

    case RelationalComparisonMode::kGreaterThan:
    case RelationalComparisonMode::kGreaterThanOrEqual:
      Return(BooleanConstant(false));
      break;
  }

  BIND(&if_equal);
  switch (mode) {
    case RelationalComparisonMode::kLessThan:
    case RelationalComparisonMode::kGreaterThan:
      Return(BooleanConstant(false));
      break;

    case RelationalComparisonMode::kLessThanOrEqual:
    case RelationalComparisonMode::kGreaterThanOrEqual:
      Return(BooleanConstant(true));
      break;
  }

  BIND(&if_greater);
  switch (mode) {
    case RelationalComparisonMode::kLessThan:
    case RelationalComparisonMode::kLessThanOrEqual:
      Return(BooleanConstant(false));
      break;

    case RelationalComparisonMode::kGreaterThan:
    case RelationalComparisonMode::kGreaterThanOrEqual:
      Return(BooleanConstant(true));
      break;
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
                                     RelationalComparisonMode::kLessThan);
}

TF_BUILTIN(StringLessThanOrEqual, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(
      context, left, right, RelationalComparisonMode::kLessThanOrEqual);
}

TF_BUILTIN(StringGreaterThan, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(context, left, right,
                                     RelationalComparisonMode::kGreaterThan);
}

TF_BUILTIN(StringGreaterThanOrEqual, StringBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  GenerateStringRelationalComparison(
      context, left, right, RelationalComparisonMode::kGreaterThanOrEqual);
}

TF_BUILTIN(StringCharAt, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);

  // Load the character code at the {position} from the {receiver}.
  Node* code = StringCharCodeAt(receiver, position, INTPTR_PARAMETERS);

  // And return the single character string with only that {code}
  Node* result = StringFromCharCode(code);
  Return(result);
}

TF_BUILTIN(StringCharCodeAt, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);

  // Load the character code at the {position} from the {receiver}.
  Node* code = StringCharCodeAt(receiver, position, INTPTR_PARAMETERS);

  // And return it as TaggedSigned value.
  // TODO(turbofan): Allow builtins to return values untagged.
  Node* result = SmiFromWord32(code);
  Return(result);
}

// -----------------------------------------------------------------------------
// ES6 section 21.1 String Objects

// ES6 #sec-string.fromcharcode
TF_BUILTIN(StringFromCharCode, CodeStubAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  CodeStubArguments arguments(this, ChangeInt32ToIntPtr(argc));
  // From now on use word-size argc value.
  argc = arguments.GetLength();

  // Check if we have exactly one argument (plus the implicit receiver), i.e.
  // if the parent frame is not an arguments adaptor frame.
  Label if_oneargument(this), if_notoneargument(this);
  Branch(WordEqual(argc, IntPtrConstant(1)), &if_oneargument,
         &if_notoneargument);

  BIND(&if_oneargument);
  {
    // Single argument case, perform fast single character string cache lookup
    // for one-byte code units, or fall back to creating a single character
    // string on the fly otherwise.
    Node* code = arguments.AtIndex(0);
    Node* code32 = TruncateTaggedToWord32(context, code);
    Node* code16 = Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));
    Node* result = StringFromCharCode(code16);
    arguments.PopAndReturn(result);
  }

  Node* code16 = nullptr;
  BIND(&if_notoneargument);
  {
    Label two_byte(this);
    // Assume that the resulting string contains only one-byte characters.
    Node* one_byte_result = AllocateSeqOneByteString(context, argc);

    VARIABLE(max_index, MachineType::PointerRepresentation());
    max_index.Bind(IntPtrConstant(0));

    // Iterate over the incoming arguments, converting them to 8-bit character
    // codes. Stop if any of the conversions generates a code that doesn't fit
    // in 8 bits.
    CodeStubAssembler::VariableList vars({&max_index}, zone());
    arguments.ForEach(vars, [this, context, &two_byte, &max_index, &code16,
                             one_byte_result](Node* arg) {
      Node* code32 = TruncateTaggedToWord32(context, arg);
      code16 = Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

      GotoIf(
          Int32GreaterThan(code16, Int32Constant(String::kMaxOneByteCharCode)),
          &two_byte);

      // The {code16} fits into the SeqOneByteString {one_byte_result}.
      Node* offset = ElementOffsetFromIndex(
          max_index.value(), UINT8_ELEMENTS,
          CodeStubAssembler::INTPTR_PARAMETERS,
          SeqOneByteString::kHeaderSize - kHeapObjectTag);
      StoreNoWriteBarrier(MachineRepresentation::kWord8, one_byte_result,
                          offset, code16);
      max_index.Bind(IntPtrAdd(max_index.value(), IntPtrConstant(1)));
    });
    arguments.PopAndReturn(one_byte_result);

    BIND(&two_byte);

    // At least one of the characters in the string requires a 16-bit
    // representation.  Allocate a SeqTwoByteString to hold the resulting
    // string.
    Node* two_byte_result = AllocateSeqTwoByteString(context, argc);

    // Copy the characters that have already been put in the 8-bit string into
    // their corresponding positions in the new 16-bit string.
    Node* zero = IntPtrConstant(0);
    CopyStringCharacters(one_byte_result, two_byte_result, zero, zero,
                         max_index.value(), String::ONE_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING,
                         CodeStubAssembler::INTPTR_PARAMETERS);

    // Write the character that caused the 8-bit to 16-bit fault.
    Node* max_index_offset =
        ElementOffsetFromIndex(max_index.value(), UINT16_ELEMENTS,
                               CodeStubAssembler::INTPTR_PARAMETERS,
                               SeqTwoByteString::kHeaderSize - kHeapObjectTag);
    StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                        max_index_offset, code16);
    max_index.Bind(IntPtrAdd(max_index.value(), IntPtrConstant(1)));

    // Resume copying the passed-in arguments from the same place where the
    // 8-bit copy stopped, but this time copying over all of the characters
    // using a 16-bit representation.
    arguments.ForEach(
        vars,
        [this, context, two_byte_result, &max_index](Node* arg) {
          Node* code32 = TruncateTaggedToWord32(context, arg);
          Node* code16 =
              Word32And(code32, Int32Constant(String::kMaxUtf16CodeUnit));

          Node* offset = ElementOffsetFromIndex(
              max_index.value(), UINT16_ELEMENTS,
              CodeStubAssembler::INTPTR_PARAMETERS,
              SeqTwoByteString::kHeaderSize - kHeapObjectTag);
          StoreNoWriteBarrier(MachineRepresentation::kWord16, two_byte_result,
                              offset, code16);
          max_index.Bind(IntPtrAdd(max_index.value(), IntPtrConstant(1)));
        },
        max_index.value());

    arguments.PopAndReturn(two_byte_result);
  }
}

// ES6 #sec-string.prototype.charat
TF_BUILTIN(StringPrototypeCharAt, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);
  Node* context = Parameter(Descriptor::kContext);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver = ToThisString(context, receiver, "String.prototype.charAt");

  // Convert the {position} to a Smi and check that it's in bounds of the
  // {receiver}.
  {
    Label return_emptystring(this, Label::kDeferred);
    position =
        ToInteger(context, position, CodeStubAssembler::kTruncateMinusZero);
    GotoIfNot(TaggedIsSmi(position), &return_emptystring);

    // Determine the actual length of the {receiver} String.
    Node* receiver_length = LoadObjectField(receiver, String::kLengthOffset);

    // Return "" if the Smi {position} is outside the bounds of the {receiver}.
    Label if_positioninbounds(this);
    Branch(SmiAboveOrEqual(position, receiver_length), &return_emptystring,
           &if_positioninbounds);

    BIND(&return_emptystring);
    Return(EmptyStringConstant());

    BIND(&if_positioninbounds);
  }

  // Load the character code at the {position} from the {receiver}.
  Node* code = StringCharCodeAt(receiver, position);

  // And return the single character string with only that {code}.
  Node* result = StringFromCharCode(code);
  Return(result);
}

// ES6 #sec-string.prototype.charcodeat
TF_BUILTIN(StringPrototypeCharCodeAt, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* position = Parameter(Descriptor::kPosition);
  Node* context = Parameter(Descriptor::kContext);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver = ToThisString(context, receiver, "String.prototype.charCodeAt");

  // Convert the {position} to a Smi and check that it's in bounds of the
  // {receiver}.
  {
    Label return_nan(this, Label::kDeferred);
    position =
        ToInteger(context, position, CodeStubAssembler::kTruncateMinusZero);
    GotoIfNot(TaggedIsSmi(position), &return_nan);

    // Determine the actual length of the {receiver} String.
    Node* receiver_length = LoadObjectField(receiver, String::kLengthOffset);

    // Return NaN if the Smi {position} is outside the bounds of the {receiver}.
    Label if_positioninbounds(this);
    Branch(SmiAboveOrEqual(position, receiver_length), &return_nan,
           &if_positioninbounds);

    BIND(&return_nan);
    Return(NaNConstant());

    BIND(&if_positioninbounds);
  }

  // Load the character at the {position} from the {receiver}.
  Node* value = StringCharCodeAt(receiver, position);
  Node* result = SmiFromWord32(value);
  Return(result);
}

// ES6 String.prototype.concat(...args)
// ES6 #sec-string.prototype.concat
TF_BUILTIN(StringPrototypeConcat, CodeStubAssembler) {
  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  CodeStubArguments arguments(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));
  Node* receiver = arguments.GetReceiver();
  Node* context = Parameter(BuiltinDescriptor::kContext);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver = ToThisString(context, receiver, "String.prototype.concat");

  // Concatenate all the arguments passed to this builtin.
  VARIABLE(var_result, MachineRepresentation::kTagged);
  var_result.Bind(receiver);
  arguments.ForEach(
      CodeStubAssembler::VariableList({&var_result}, zone()),
      [this, context, &var_result](Node* arg) {
        arg = CallStub(CodeFactory::ToString(isolate()), context, arg);
        var_result.Bind(CallStub(CodeFactory::StringAdd(isolate()), context,
                                 var_result.value(), arg));
      });
  arguments.PopAndReturn(var_result.value());
}

void StringBuiltinsAssembler::StringIndexOf(
    Node* receiver, Node* instance_type, Node* search_string,
    Node* search_string_instance_type, Node* position,
    std::function<void(Node*)> f_return) {
  CSA_ASSERT(this, IsString(receiver));
  CSA_ASSERT(this, IsString(search_string));
  CSA_ASSERT(this, TaggedIsSmi(position));

  Label zero_length_needle(this),
      call_runtime_unchecked(this, Label::kDeferred), return_minus_1(this),
      check_search_string(this), continue_fast_path(this);

  Node* const int_zero = IntPtrConstant(0);
  VARIABLE(var_needle_byte, MachineType::PointerRepresentation(), int_zero);
  VARIABLE(var_string_addr, MachineType::PointerRepresentation(), int_zero);

  Node* needle_length = SmiUntag(LoadStringLength(search_string));
  // Use faster/complex runtime fallback for long search strings.
  GotoIf(IntPtrLessThan(IntPtrConstant(1), needle_length),
         &call_runtime_unchecked);
  Node* string_length = SmiUntag(LoadStringLength(receiver));
  Node* start_position = IntPtrMax(SmiUntag(position), int_zero);

  GotoIf(IntPtrEqual(int_zero, needle_length), &zero_length_needle);
  // Check that the needle fits in the start position.
  GotoIfNot(IntPtrLessThanOrEqual(needle_length,
                                  IntPtrSub(string_length, start_position)),
            &return_minus_1);

  // Load the string address.
  {
    Label if_onebyte_sequential(this);
    Label if_onebyte_external(this, Label::kDeferred);

    // Only support one-byte strings on the fast path.
    DispatchOnStringInstanceType(instance_type, &if_onebyte_sequential,
                                 &if_onebyte_external, &call_runtime_unchecked);

    BIND(&if_onebyte_sequential);
    {
      var_string_addr.Bind(
          OneByteCharAddress(BitcastTaggedToWord(receiver), start_position));
      Goto(&check_search_string);
    }

    BIND(&if_onebyte_external);
    {
      Node* const unpacked = TryDerefExternalString(receiver, instance_type,
                                                    &call_runtime_unchecked);
      var_string_addr.Bind(OneByteCharAddress(unpacked, start_position));
      Goto(&check_search_string);
    }
  }

  // Load the needle character.
  BIND(&check_search_string);
  {
    Label if_onebyte_sequential(this);
    Label if_onebyte_external(this, Label::kDeferred);

    DispatchOnStringInstanceType(search_string_instance_type,
                                 &if_onebyte_sequential, &if_onebyte_external,
                                 &call_runtime_unchecked);

    BIND(&if_onebyte_sequential);
    {
      var_needle_byte.Bind(
          ChangeInt32ToIntPtr(LoadOneByteChar(search_string, int_zero)));
      Goto(&continue_fast_path);
    }

    BIND(&if_onebyte_external);
    {
      Node* const unpacked = TryDerefExternalString(
          search_string, search_string_instance_type, &call_runtime_unchecked);
      var_needle_byte.Bind(
          ChangeInt32ToIntPtr(LoadOneByteChar(unpacked, int_zero)));
      Goto(&continue_fast_path);
    }
  }

  BIND(&continue_fast_path);
  {
    Node* needle_byte = var_needle_byte.value();
    Node* string_addr = var_string_addr.value();
    Node* search_length = IntPtrSub(string_length, start_position);
    // Call out to the highly optimized memchr to perform the actual byte
    // search.
    Node* memchr =
        ExternalConstant(ExternalReference::libc_memchr_function(isolate()));
    Node* result_address =
        CallCFunction3(MachineType::Pointer(), MachineType::Pointer(),
                       MachineType::IntPtr(), MachineType::UintPtr(), memchr,
                       string_addr, needle_byte, search_length);
    GotoIf(WordEqual(result_address, int_zero), &return_minus_1);
    Node* result_index =
        IntPtrAdd(IntPtrSub(result_address, string_addr), start_position);
    f_return(SmiTag(result_index));
  }

  BIND(&return_minus_1);
  f_return(SmiConstant(-1));

  BIND(&zero_length_needle);
  {
    Comment("0-length search_string");
    f_return(SmiTag(IntPtrMin(string_length, start_position)));
  }

  BIND(&call_runtime_unchecked);
  {
    // Simplified version of the runtime call where the types of the arguments
    // are already known due to type checks in this stub.
    Comment("Call Runtime Unchecked");
    Node* result = CallRuntime(Runtime::kStringIndexOfUnchecked, SmiConstant(0),
                               receiver, search_string, position);
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

  Node* instance_type = LoadInstanceType(receiver);
  Node* search_string_instance_type = LoadInstanceType(search_string);

  StringIndexOf(receiver, instance_type, search_string,
                search_string_instance_type, position,
                [this](Node* result) { this->Return(result); });
}

// ES6 String.prototype.indexOf(searchString [, position])
// #sec-string.prototype.indexof
TF_BUILTIN(StringPrototypeIndexOf, StringBuiltinsAssembler) {
  VARIABLE(search_string, MachineRepresentation::kTagged);
  VARIABLE(position, MachineRepresentation::kTagged);
  Label call_runtime(this), call_runtime_unchecked(this), argc_0(this),
      no_argc_0(this), argc_1(this), no_argc_1(this), argc_2(this),
      fast_path(this), return_minus_1(this);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  CodeStubArguments arguments(this, ChangeInt32ToIntPtr(argc));
  Node* receiver = arguments.GetReceiver();
  // From now on use word-size argc value.
  argc = arguments.GetLength();

  GotoIf(IntPtrEqual(argc, IntPtrConstant(0)), &argc_0);
  GotoIf(IntPtrEqual(argc, IntPtrConstant(1)), &argc_1);
  Goto(&argc_2);
  BIND(&argc_0);
  {
    Comment("0 Argument case");
    Node* undefined = UndefinedConstant();
    search_string.Bind(undefined);
    position.Bind(undefined);
    Goto(&call_runtime);
  }
  BIND(&argc_1);
  {
    Comment("1 Argument case");
    search_string.Bind(arguments.AtIndex(0));
    position.Bind(SmiConstant(0));
    Goto(&fast_path);
  }
  BIND(&argc_2);
  {
    Comment("2 Argument case");
    search_string.Bind(arguments.AtIndex(0));
    position.Bind(arguments.AtIndex(1));
    GotoIfNot(TaggedIsSmi(position.value()), &call_runtime);
    Goto(&fast_path);
  }

  BIND(&fast_path);
  {
    Comment("Fast Path");
    GotoIf(TaggedIsSmi(receiver), &call_runtime);
    Node* needle = search_string.value();
    GotoIf(TaggedIsSmi(needle), &call_runtime);

    Node* instance_type = LoadInstanceType(receiver);
    GotoIfNot(IsStringInstanceType(instance_type), &call_runtime);

    Node* needle_instance_type = LoadInstanceType(needle);
    GotoIfNot(IsStringInstanceType(needle_instance_type), &call_runtime);

    StringIndexOf(
        receiver, instance_type, needle, needle_instance_type, position.value(),
        [&arguments](Node* result) { arguments.PopAndReturn(result); });
  }

  BIND(&call_runtime);
  {
    Comment("Call Runtime");
    Node* result = CallRuntime(Runtime::kStringIndexOf, context, receiver,
                               search_string.value(), position.value());
    arguments.PopAndReturn(result);
  }
}

compiler::Node* StringBuiltinsAssembler::IsNullOrUndefined(Node* const value) {
  return Word32Or(IsUndefined(value), IsNull(value));
}

void StringBuiltinsAssembler::RequireObjectCoercible(Node* const context,
                                                     Node* const value,
                                                     const char* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  Branch(IsNullOrUndefined(value), &throw_exception, &out);

  BIND(&throw_exception);
  TailCallRuntime(
      Runtime::kThrowCalledOnNullOrUndefined, context,
      HeapConstant(factory()->NewStringFromAsciiChecked(method_name, TENURED)));

  BIND(&out);
}

void StringBuiltinsAssembler::MaybeCallFunctionAtSymbol(
    Node* const context, Node* const object, Handle<Symbol> symbol,
    const NodeFunction0& regexp_call, const NodeFunction1& generic_call) {
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
  {
    Label stub_call(this), slow_lookup(this);

    RegExpBuiltinsAssembler regexp_asm(state());
    regexp_asm.BranchIfFastRegExp(context, object, object_map, &stub_call,
                                  &slow_lookup);

    BIND(&stub_call);
    Return(regexp_call());

    BIND(&slow_lookup);
  }

  GotoIf(IsNullOrUndefined(object), &out);

  // Fall back to a slow lookup of {object[symbol]}.

  Node* const maybe_func = GetProperty(context, object, symbol);
  GotoIf(IsUndefined(maybe_func), &out);

  // Attempt to call the function.

  Node* const result = generic_call(maybe_func);
  Return(result);

  BIND(&out);
}

// ES6 #sec-string.prototype.replace
TF_BUILTIN(StringPrototypeReplace, StringBuiltinsAssembler) {
  Label out(this);

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const search = Parameter(Descriptor::kSearch);
  Node* const replace = Parameter(Descriptor::kReplace);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.replace");

  // Redirect to replacer method if {search[@@replace]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, search, isolate()->factory()->replace_symbol(),
      [=]() {
        Callable tostring_callable = CodeFactory::ToString(isolate());
        Node* const subject_string =
            CallStub(tostring_callable, context, receiver);

        Callable replace_callable = CodeFactory::RegExpReplace(isolate());
        return CallStub(replace_callable, context, search, subject_string,
                        replace);
      },
      [=](Node* fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        return CallJS(call_callable, context, fn, search, receiver, replace);
      });

  // Convert {receiver} and {search} to strings.

  Callable tostring_callable = CodeFactory::ToString(isolate());
  Callable indexof_callable = CodeFactory::StringIndexOf(isolate());

  Node* const subject_string = CallStub(tostring_callable, context, receiver);
  Node* const search_string = CallStub(tostring_callable, context, search);

  Node* const subject_length = LoadStringLength(subject_string);
  Node* const search_length = LoadStringLength(search_string);

  // Fast-path single-char {search}, long {receiver}, and simple string
  // {replace}.
  {
    Label next(this);

    GotoIfNot(SmiEqual(search_length, SmiConstant(1)), &next);
    GotoIfNot(SmiGreaterThan(subject_length, SmiConstant(0xFF)), &next);
    GotoIf(TaggedIsSmi(replace), &next);
    GotoIfNot(IsString(replace), &next);

    Node* const dollar_string = HeapConstant(
        isolate()->factory()->LookupSingleCharacterStringFromCode('$'));
    Node* const dollar_ix =
        CallStub(indexof_callable, context, replace, dollar_string, smi_zero);
    GotoIfNot(SmiIsNegative(dollar_ix), &next);

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

  Node* const match_start_index = CallStub(
      indexof_callable, context, subject_string, search_string, smi_zero);
  CSA_ASSERT(this, TaggedIsSmi(match_start_index));

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
    CallStub(tostring_callable, context, replace);
    Goto(&return_subject);

    BIND(&return_subject);
    Return(subject_string);

    BIND(&next);
  }

  Node* const match_end_index = SmiAdd(match_start_index, search_length);

  Callable substring_callable = CodeFactory::SubString(isolate());
  Callable stringadd_callable =
      CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);

  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  // Compute the prefix.
  {
    Label next(this);

    GotoIf(SmiEqual(match_start_index, smi_zero), &next);
    Node* const prefix = CallStub(substring_callable, context, subject_string,
                                  smi_zero, match_start_index);
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
    Node* const replacement_string =
        CallStub(tostring_callable, context, replacement);
    var_result.Bind(CallStub(stringadd_callable, context, var_result.value(),
                             replacement_string));
    Goto(&out);
  }

  BIND(&if_notcallablereplace);
  {
    Node* const replace_string = CallStub(tostring_callable, context, replace);

    // TODO(jgruber): Simplified GetSubstitution implementation in CSA.
    Node* const matched = CallStub(substring_callable, context, subject_string,
                                   match_start_index, match_end_index);
    Node* const replacement_string =
        CallRuntime(Runtime::kGetSubstitution, context, matched, subject_string,
                    match_start_index, replace_string);
    var_result.Bind(CallStub(stringadd_callable, context, var_result.value(),
                             replacement_string));
    Goto(&out);
  }

  BIND(&out);
  {
    Node* const suffix = CallStub(substring_callable, context, subject_string,
                                  match_end_index, subject_length);
    Node* const result =
        CallStub(stringadd_callable, context, var_result.value(), suffix);
    Return(result);
  }
}

// ES6 section 21.1.3.19 String.prototype.split ( separator, limit )
TF_BUILTIN(StringPrototypeSplit, StringBuiltinsAssembler) {
  Label out(this);

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const separator = Parameter(Descriptor::kSeparator);
  Node* const limit = Parameter(Descriptor::kLimit);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const smi_zero = SmiConstant(0);

  RequireObjectCoercible(context, receiver, "String.prototype.split");

  // Redirect to splitter method if {separator[@@split]} is not undefined.

  MaybeCallFunctionAtSymbol(
      context, separator, isolate()->factory()->split_symbol(),
      [=]() {
        Callable tostring_callable = CodeFactory::ToString(isolate());
        Node* const subject_string =
            CallStub(tostring_callable, context, receiver);

        Callable split_callable = CodeFactory::RegExpSplit(isolate());
        return CallStub(split_callable, context, separator, subject_string,
                        limit);
      },
      [=](Node* fn) {
        Callable call_callable = CodeFactory::Call(isolate());
        return CallJS(call_callable, context, fn, separator, receiver, limit);
      });

  // String and integer conversions.
  // TODO(jgruber): The old implementation used Uint32Max instead of SmiMax -
  // but AFAIK there should not be a difference since arrays are capped at Smi
  // lengths.

  Callable tostring_callable = CodeFactory::ToString(isolate());
  Node* const subject_string = CallStub(tostring_callable, context, receiver);
  Node* const limit_number =
      Select(IsUndefined(limit), [=]() { return SmiConstant(Smi::kMaxValue); },
             [=]() { return ToUint32(context, limit); },
             MachineRepresentation::kTagged);
  Node* const separator_string =
      CallStub(tostring_callable, context, separator);

  // Shortcut for {limit} == 0.
  {
    Label next(this);
    GotoIfNot(SmiEqual(limit_number, smi_zero), &next);

    const ElementsKind kind = FAST_ELEMENTS;
    Node* const native_context = LoadNativeContext(context);
    Node* const array_map = LoadJSArrayElementsMap(kind, native_context);

    Node* const length = smi_zero;
    Node* const capacity = IntPtrConstant(0);
    Node* const result = AllocateJSArray(kind, array_map, capacity, length);

    Return(result);

    BIND(&next);
  }

  // ECMA-262 says that if {separator} is undefined, the result should
  // be an array of size 1 containing the entire string.
  {
    Label next(this);
    GotoIfNot(IsUndefined(separator), &next);

    const ElementsKind kind = FAST_ELEMENTS;
    Node* const native_context = LoadNativeContext(context);
    Node* const array_map = LoadJSArrayElementsMap(kind, native_context);

    Node* const length = SmiConstant(1);
    Node* const capacity = IntPtrConstant(1);
    Node* const result = AllocateJSArray(kind, array_map, capacity, length);

    Node* const fixed_array = LoadElements(result);
    StoreFixedArrayElement(fixed_array, 0, subject_string);

    Return(result);

    BIND(&next);
  }

  // If the separator string is empty then return the elements in the subject.
  {
    Label next(this);
    GotoIfNot(SmiEqual(LoadStringLength(separator_string), smi_zero), &next);

    Node* const result = CallRuntime(Runtime::kStringToArray, context,
                                     subject_string, limit_number);
    Return(result);

    BIND(&next);
  }

  Node* const result =
      CallRuntime(Runtime::kStringSplit, context, subject_string,
                  separator_string, limit_number);
  Return(result);
}

// ES6 #sec-string.prototype.substr
TF_BUILTIN(StringPrototypeSubstr, CodeStubAssembler) {
  Label out(this), handle_length(this);

  VARIABLE(var_start, MachineRepresentation::kTagged);
  VARIABLE(var_length, MachineRepresentation::kTagged);

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const start = Parameter(Descriptor::kStart);
  Node* const length = Parameter(Descriptor::kLength);
  Node* const context = Parameter(Descriptor::kContext);

  Node* const zero = SmiConstant(Smi::kZero);

  // Check that {receiver} is coercible to Object and convert it to a String.
  Node* const string =
      ToThisString(context, receiver, "String.prototype.substr");

  Node* const string_length = LoadStringLength(string);

  // Conversions and bounds-checks for {start}.
  {
    Node* const start_int =
        ToInteger(context, start, CodeStubAssembler::kTruncateMinusZero);

    Label if_issmi(this), if_isheapnumber(this, Label::kDeferred);
    Branch(TaggedIsSmi(start_int), &if_issmi, &if_isheapnumber);

    BIND(&if_issmi);
    {
      Node* const length_plus_start = SmiAdd(string_length, start_int);
      var_start.Bind(Select(SmiLessThan(start_int, zero),
                            [&] { return SmiMax(length_plus_start, zero); },
                            [&] { return start_int; },
                            MachineRepresentation::kTagged));
      Goto(&handle_length);
    }

    BIND(&if_isheapnumber);
    {
      // If {start} is a heap number, it is definitely out of bounds. If it is
      // negative, {start} = max({string_length} + {start}),0) = 0'. If it is
      // positive, set {start} to {string_length} which ultimately results in
      // returning an empty string.
      Node* const float_zero = Float64Constant(0.);
      Node* const start_float = LoadHeapNumberValue(start_int);
      var_start.Bind(SelectTaggedConstant(
          Float64LessThan(start_float, float_zero), zero, string_length));
      Goto(&handle_length);
    }
  }

  // Conversions and bounds-checks for {length}.
  BIND(&handle_length);
  {
    Label if_issmi(this), if_isheapnumber(this, Label::kDeferred);

    // Default to {string_length} if {length} is undefined.
    {
      Label if_isundefined(this, Label::kDeferred), if_isnotundefined(this);
      Branch(WordEqual(length, UndefinedConstant()), &if_isundefined,
             &if_isnotundefined);

      BIND(&if_isundefined);
      var_length.Bind(string_length);
      Goto(&if_issmi);

      BIND(&if_isnotundefined);
      var_length.Bind(
          ToInteger(context, length, CodeStubAssembler::kTruncateMinusZero));
    }

    Branch(TaggedIsSmi(var_length.value()), &if_issmi, &if_isheapnumber);

    // Set {length} to min(max({length}, 0), {string_length} - {start}
    BIND(&if_issmi);
    {
      Node* const positive_length = SmiMax(var_length.value(), zero);

      Node* const minimal_length = SmiSub(string_length, var_start.value());
      var_length.Bind(SmiMin(positive_length, minimal_length));

      GotoIfNot(SmiLessThanOrEqual(var_length.value(), zero), &out);
      Return(EmptyStringConstant());
    }

    BIND(&if_isheapnumber);
    {
      // If {length} is a heap number, it is definitely out of bounds. There are
      // two cases according to the spec: if it is negative, "" is returned; if
      // it is positive, then length is set to {string_length} - {start}.

      CSA_ASSERT(this, IsHeapNumberMap(LoadMap(var_length.value())));

      Label if_isnegative(this), if_ispositive(this);
      Node* const float_zero = Float64Constant(0.);
      Node* const length_float = LoadHeapNumberValue(var_length.value());
      Branch(Float64LessThan(length_float, float_zero), &if_isnegative,
             &if_ispositive);

      BIND(&if_isnegative);
      Return(EmptyStringConstant());

      BIND(&if_ispositive);
      {
        var_length.Bind(SmiSub(string_length, var_start.value()));
        GotoIfNot(SmiLessThanOrEqual(var_length.value(), zero), &out);
        Return(EmptyStringConstant());
      }
    }
  }

  BIND(&out);
  {
    Node* const end = SmiAdd(var_start.value(), var_length.value());
    Node* const result = SubString(context, string, var_start.value(), end);
    Return(result);
  }
}

compiler::Node* StringBuiltinsAssembler::ToSmiBetweenZeroAnd(Node* context,
                                                             Node* value,
                                                             Node* limit) {
  Label out(this);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  Node* const value_int =
      this->ToInteger(context, value, CodeStubAssembler::kTruncateMinusZero);

  Label if_issmi(this), if_isnotsmi(this, Label::kDeferred);
  Branch(TaggedIsSmi(value_int), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    Label if_isinbounds(this), if_isoutofbounds(this, Label::kDeferred);
    Branch(SmiAbove(value_int, limit), &if_isoutofbounds, &if_isinbounds);

    BIND(&if_isinbounds);
    {
      var_result.Bind(value_int);
      Goto(&out);
    }

    BIND(&if_isoutofbounds);
    {
      Node* const zero = SmiConstant(Smi::kZero);
      var_result.Bind(
          SelectTaggedConstant(SmiLessThan(value_int, zero), zero, limit));
      Goto(&out);
    }
  }

  BIND(&if_isnotsmi);
  {
    // {value} is a heap number - in this case, it is definitely out of bounds.
    CSA_ASSERT(this, IsHeapNumberMap(LoadMap(value_int)));

    Node* const float_zero = Float64Constant(0.);
    Node* const smi_zero = SmiConstant(Smi::kZero);
    Node* const value_float = LoadHeapNumberValue(value_int);
    var_result.Bind(SelectTaggedConstant(
        Float64LessThan(value_float, float_zero), smi_zero, limit));
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES6 #sec-string.prototype.substring
TF_BUILTIN(StringPrototypeSubstring, StringBuiltinsAssembler) {
  Label out(this);

  VARIABLE(var_start, MachineRepresentation::kTagged);
  VARIABLE(var_end, MachineRepresentation::kTagged);

  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const start = Parameter(Descriptor::kStart);
  Node* const end = Parameter(Descriptor::kEnd);
  Node* const context = Parameter(Descriptor::kContext);

  // Check that {receiver} is coercible to Object and convert it to a String.
  Node* const string =
      ToThisString(context, receiver, "String.prototype.substring");

  Node* const length = LoadStringLength(string);

  // Conversion and bounds-checks for {start}.
  var_start.Bind(ToSmiBetweenZeroAnd(context, start, length));

  // Conversion and bounds-checks for {end}.
  {
    var_end.Bind(length);
    GotoIf(WordEqual(end, UndefinedConstant()), &out);

    var_end.Bind(ToSmiBetweenZeroAnd(context, end, length));

    Label if_endislessthanstart(this);
    Branch(SmiLessThan(var_end.value(), var_start.value()),
           &if_endislessthanstart, &out);

    BIND(&if_endislessthanstart);
    {
      Node* const tmp = var_end.value();
      var_end.Bind(var_start.value());
      var_start.Bind(tmp);
      Goto(&out);
    }
  }

  BIND(&out);
  {
    Node* result =
        SubString(context, string, var_start.value(), var_end.value());
    Return(result);
  }
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
  StoreObjectFieldRoot(iterator, JSValue::kPropertiesOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(iterator, JSObject::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kStringOffset,
                                 string);
  Node* index = SmiConstant(Smi::kZero);
  StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kNextIndexOffset,
                                 index);
  Return(iterator);
}

// Return the |word32| codepoint at {index}. Supports SeqStrings and
// ExternalStrings.
compiler::Node* StringBuiltinsAssembler::LoadSurrogatePairAt(
    compiler::Node* string, compiler::Node* length, compiler::Node* index,
    UnicodeEncoding encoding) {
  Label handle_surrogate_pair(this), return_result(this);
  VARIABLE(var_result, MachineRepresentation::kWord32);
  VARIABLE(var_trail, MachineRepresentation::kWord32);
  var_result.Bind(StringCharCodeAt(string, index));
  var_trail.Bind(Int32Constant(0));

  GotoIf(Word32NotEqual(Word32And(var_result.value(), Int32Constant(0xFC00)),
                        Int32Constant(0xD800)),
         &return_result);
  Node* next_index = SmiAdd(index, SmiConstant(Smi::FromInt(1)));

  GotoIfNot(SmiLessThan(next_index, length), &return_result);
  var_trail.Bind(StringCharCodeAt(string, next_index));
  Branch(Word32Equal(Word32And(var_trail.value(), Int32Constant(0xFC00)),
                     Int32Constant(0xDC00)),
         &handle_surrogate_pair, &return_result);

  BIND(&handle_surrogate_pair);
  {
    Node* lead = var_result.value();
    Node* trail = var_trail.value();

    // Check that this path is only taken if a surrogate pair is found
    CSA_SLOW_ASSERT(this,
                    Uint32GreaterThanOrEqual(lead, Int32Constant(0xD800)));
    CSA_SLOW_ASSERT(this, Uint32LessThan(lead, Int32Constant(0xDC00)));
    CSA_SLOW_ASSERT(this,
                    Uint32GreaterThanOrEqual(trail, Int32Constant(0xDC00)));
    CSA_SLOW_ASSERT(this, Uint32LessThan(trail, Int32Constant(0xE000)));

    switch (encoding) {
      case UnicodeEncoding::UTF16:
        var_result.Bind(Word32Or(
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
        Node* surrogate_offset =
            Int32Constant(0x10000 - (0xD800 << 10) - 0xDC00);

        // (lead << 10) + trail + SURROGATE_OFFSET
        var_result.Bind(Int32Add(WordShl(lead, Int32Constant(10)),
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
  var_done.Bind(BooleanConstant(true));

  Label throw_bad_receiver(this), next_codepoint(this), return_result(this);

  Node* context = Parameter(Descriptor::kContext);
  Node* iterator = Parameter(Descriptor::kReceiver);

  GotoIf(TaggedIsSmi(iterator), &throw_bad_receiver);
  GotoIfNot(Word32Equal(LoadInstanceType(iterator),
                        Int32Constant(JS_STRING_ITERATOR_TYPE)),
            &throw_bad_receiver);

  Node* string = LoadObjectField(iterator, JSStringIterator::kStringOffset);
  Node* position =
      LoadObjectField(iterator, JSStringIterator::kNextIndexOffset);
  Node* length = LoadObjectField(string, String::kLengthOffset);

  Branch(SmiLessThan(position, length), &next_codepoint, &return_result);

  BIND(&next_codepoint);
  {
    UnicodeEncoding encoding = UnicodeEncoding::UTF16;
    Node* ch = LoadSurrogatePairAt(string, length, position, encoding);
    Node* value = StringFromCodePoint(ch, encoding);
    var_value.Bind(value);
    Node* length = LoadObjectField(value, String::kLengthOffset);
    StoreObjectFieldNoWriteBarrier(iterator, JSStringIterator::kNextIndexOffset,
                                   SmiAdd(position, length));
    var_done.Bind(BooleanConstant(false));
    Goto(&return_result);
  }

  BIND(&return_result);
  {
    Node* native_context = LoadNativeContext(context);
    Node* map =
        LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
    Node* result = Allocate(JSIteratorResult::kSize);
    StoreMapNoWriteBarrier(result, map);
    StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                         Heap::kEmptyFixedArrayRootIndex);
    StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset,
                                   var_value.value());
    StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset,
                                   var_done.value());
    Return(result);
  }

  BIND(&throw_bad_receiver);
  {
    // The {receiver} is not a valid JSGeneratorObject.
    CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
                HeapConstant(factory()->NewStringFromAsciiChecked(
                    "String Iterator.prototype.next", TENURED)),
                iterator);
    Unreachable();
  }
}

}  // namespace internal
}  // namespace v8
