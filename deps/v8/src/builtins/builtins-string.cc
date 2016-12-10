// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/code-factory.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 21.1 String Objects

// ES6 section 21.1.2.1 String.fromCharCode ( ...codeUnits )
void Builtins::Generate_StringFromCharCode(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* code = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);

  // Check if we have exactly one argument (plus the implicit receiver), i.e.
  // if the parent frame is not an arguments adaptor frame.
  Label if_oneargument(assembler), if_notoneargument(assembler);
  Node* parent_frame_pointer = assembler->LoadParentFramePointer();
  Node* parent_frame_type =
      assembler->Load(MachineType::Pointer(), parent_frame_pointer,
                      assembler->IntPtrConstant(
                          CommonFrameConstants::kContextOrFrameTypeOffset));
  assembler->Branch(
      assembler->WordEqual(
          parent_frame_type,
          assembler->SmiConstant(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR))),
      &if_notoneargument, &if_oneargument);

  assembler->Bind(&if_oneargument);
  {
    // Single argument case, perform fast single character string cache lookup
    // for one-byte code units, or fall back to creating a single character
    // string on the fly otherwise.
    Node* code32 = assembler->TruncateTaggedToWord32(context, code);
    Node* code16 = assembler->Word32And(
        code32, assembler->Int32Constant(String::kMaxUtf16CodeUnit));
    Node* result = assembler->StringFromCharCode(code16);
    assembler->Return(result);
  }

  assembler->Bind(&if_notoneargument);
  {
    // Determine the resulting string length.
    Node* length = assembler->LoadAndUntagSmi(
        parent_frame_pointer, ArgumentsAdaptorFrameConstants::kLengthOffset);

    // Assume that the resulting string contains only one-byte characters.
    Node* result = assembler->AllocateSeqOneByteString(context, length);

    // Truncate all input parameters and append them to the resulting string.
    Variable var_offset(assembler, MachineType::PointerRepresentation());
    Label loop(assembler, &var_offset), done_loop(assembler);
    var_offset.Bind(assembler->IntPtrConstant(0));
    assembler->Goto(&loop);
    assembler->Bind(&loop);
    {
      // Load the current {offset}.
      Node* offset = var_offset.value();

      // Check if we're done with the string.
      assembler->GotoIf(assembler->WordEqual(offset, length), &done_loop);

      // Load the next code point and truncate it to a 16-bit value.
      Node* code = assembler->Load(
          MachineType::AnyTagged(), parent_frame_pointer,
          assembler->IntPtrAdd(
              assembler->WordShl(assembler->IntPtrSub(length, offset),
                                 assembler->IntPtrConstant(kPointerSizeLog2)),
              assembler->IntPtrConstant(
                  CommonFrameConstants::kFixedFrameSizeAboveFp -
                  kPointerSize)));
      Node* code32 = assembler->TruncateTaggedToWord32(context, code);
      Node* code16 = assembler->Word32And(
          code32, assembler->Int32Constant(String::kMaxUtf16CodeUnit));

      // Check if {code16} fits into a one-byte string.
      Label if_codeisonebyte(assembler), if_codeistwobyte(assembler);
      assembler->Branch(
          assembler->Int32LessThanOrEqual(
              code16, assembler->Int32Constant(String::kMaxOneByteCharCode)),
          &if_codeisonebyte, &if_codeistwobyte);

      assembler->Bind(&if_codeisonebyte);
      {
        // The {code16} fits into the SeqOneByteString {result}.
        assembler->StoreNoWriteBarrier(
            MachineRepresentation::kWord8, result,
            assembler->IntPtrAdd(
                assembler->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                          kHeapObjectTag),
                offset),
            code16);
        var_offset.Bind(
            assembler->IntPtrAdd(offset, assembler->IntPtrConstant(1)));
        assembler->Goto(&loop);
      }

      assembler->Bind(&if_codeistwobyte);
      {
        // Allocate a SeqTwoByteString to hold the resulting string.
        Node* cresult = assembler->AllocateSeqTwoByteString(context, length);

        // Copy all characters that were previously written to the
        // SeqOneByteString in {result} over to the new {cresult}.
        Variable var_coffset(assembler, MachineType::PointerRepresentation());
        Label cloop(assembler, &var_coffset), done_cloop(assembler);
        var_coffset.Bind(assembler->IntPtrConstant(0));
        assembler->Goto(&cloop);
        assembler->Bind(&cloop);
        {
          Node* coffset = var_coffset.value();
          assembler->GotoIf(assembler->WordEqual(coffset, offset), &done_cloop);
          Node* ccode = assembler->Load(
              MachineType::Uint8(), result,
              assembler->IntPtrAdd(
                  assembler->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                            kHeapObjectTag),
                  coffset));
          assembler->StoreNoWriteBarrier(
              MachineRepresentation::kWord16, cresult,
              assembler->IntPtrAdd(
                  assembler->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                            kHeapObjectTag),
                  assembler->WordShl(coffset, 1)),
              ccode);
          var_coffset.Bind(
              assembler->IntPtrAdd(coffset, assembler->IntPtrConstant(1)));
          assembler->Goto(&cloop);
        }

        // Write the pending {code16} to {offset}.
        assembler->Bind(&done_cloop);
        assembler->StoreNoWriteBarrier(
            MachineRepresentation::kWord16, cresult,
            assembler->IntPtrAdd(
                assembler->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                          kHeapObjectTag),
                assembler->WordShl(offset, 1)),
            code16);

        // Copy the remaining parameters to the SeqTwoByteString {cresult}.
        Label floop(assembler, &var_offset), done_floop(assembler);
        assembler->Goto(&floop);
        assembler->Bind(&floop);
        {
          // Compute the next {offset}.
          Node* offset = assembler->IntPtrAdd(var_offset.value(),
                                              assembler->IntPtrConstant(1));

          // Check if we're done with the string.
          assembler->GotoIf(assembler->WordEqual(offset, length), &done_floop);

          // Load the next code point and truncate it to a 16-bit value.
          Node* code = assembler->Load(
              MachineType::AnyTagged(), parent_frame_pointer,
              assembler->IntPtrAdd(
                  assembler->WordShl(
                      assembler->IntPtrSub(length, offset),
                      assembler->IntPtrConstant(kPointerSizeLog2)),
                  assembler->IntPtrConstant(
                      CommonFrameConstants::kFixedFrameSizeAboveFp -
                      kPointerSize)));
          Node* code32 = assembler->TruncateTaggedToWord32(context, code);
          Node* code16 = assembler->Word32And(
              code32, assembler->Int32Constant(String::kMaxUtf16CodeUnit));

          // Store the truncated {code} point at the next offset.
          assembler->StoreNoWriteBarrier(
              MachineRepresentation::kWord16, cresult,
              assembler->IntPtrAdd(
                  assembler->IntPtrConstant(SeqTwoByteString::kHeaderSize -
                                            kHeapObjectTag),
                  assembler->WordShl(offset, 1)),
              code16);
          var_offset.Bind(offset);
          assembler->Goto(&floop);
        }

        // Return the SeqTwoByteString.
        assembler->Bind(&done_floop);
        assembler->Return(cresult);
      }
    }

    assembler->Bind(&done_loop);
    assembler->Return(result);
  }
}

namespace {  // for String.fromCodePoint

bool IsValidCodePoint(Isolate* isolate, Handle<Object> value) {
  if (!value->IsNumber() && !Object::ToNumber(value).ToHandle(&value)) {
    return false;
  }

  if (Object::ToInteger(isolate, value).ToHandleChecked()->Number() !=
      value->Number()) {
    return false;
  }

  if (value->Number() < 0 || value->Number() > 0x10FFFF) {
    return false;
  }

  return true;
}

uc32 NextCodePoint(Isolate* isolate, BuiltinArguments args, int index) {
  Handle<Object> value = args.at<Object>(1 + index);
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value, Object::ToNumber(value), -1);
  if (!IsValidCodePoint(isolate, value)) {
    isolate->Throw(*isolate->factory()->NewRangeError(
        MessageTemplate::kInvalidCodePoint, value));
    return -1;
  }
  return DoubleToUint32(value->Number());
}

}  // namespace

// ES6 section 21.1.2.2 String.fromCodePoint ( ...codePoints )
BUILTIN(StringFromCodePoint) {
  HandleScope scope(isolate);
  int const length = args.length() - 1;
  if (length == 0) return isolate->heap()->empty_string();
  DCHECK_LT(0, length);

  // Optimistically assume that the resulting String contains only one byte
  // characters.
  List<uint8_t> one_byte_buffer(length);
  uc32 code = 0;
  int index;
  for (index = 0; index < length; index++) {
    code = NextCodePoint(isolate, args, index);
    if (code < 0) {
      return isolate->heap()->exception();
    }
    if (code > String::kMaxOneByteCharCode) {
      break;
    }
    one_byte_buffer.Add(code);
  }

  if (index == length) {
    RETURN_RESULT_OR_FAILURE(isolate, isolate->factory()->NewStringFromOneByte(
                                          one_byte_buffer.ToConstVector()));
  }

  List<uc16> two_byte_buffer(length - index);

  while (true) {
    if (code <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
      two_byte_buffer.Add(code);
    } else {
      two_byte_buffer.Add(unibrow::Utf16::LeadSurrogate(code));
      two_byte_buffer.Add(unibrow::Utf16::TrailSurrogate(code));
    }

    if (++index == length) {
      break;
    }
    code = NextCodePoint(isolate, args, index);
    if (code < 0) {
      return isolate->heap()->exception();
    }
  }

  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewRawTwoByteString(one_byte_buffer.length() +
                                              two_byte_buffer.length()));

  CopyChars(result->GetChars(), one_byte_buffer.ToConstVector().start(),
            one_byte_buffer.length());
  CopyChars(result->GetChars() + one_byte_buffer.length(),
            two_byte_buffer.ToConstVector().start(), two_byte_buffer.length());

  return *result;
}

// ES6 section 21.1.3.1 String.prototype.charAt ( pos )
void Builtins::Generate_StringPrototypeCharAt(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* receiver = assembler->Parameter(0);
  Node* position = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver =
      assembler->ToThisString(context, receiver, "String.prototype.charAt");

  // Convert the {position} to a Smi and check that it's in bounds of the
  // {receiver}.
  // TODO(bmeurer): Find an abstraction for this!
  {
    // Check if the {position} is already a Smi.
    Variable var_position(assembler, MachineRepresentation::kTagged);
    var_position.Bind(position);
    Label if_positionissmi(assembler),
        if_positionisnotsmi(assembler, Label::kDeferred);
    assembler->Branch(assembler->WordIsSmi(position), &if_positionissmi,
                      &if_positionisnotsmi);
    assembler->Bind(&if_positionisnotsmi);
    {
      // Convert the {position} to an Integer via the ToIntegerStub.
      Callable callable = CodeFactory::ToInteger(assembler->isolate());
      Node* index = assembler->CallStub(callable, context, position);

      // Check if the resulting {index} is now a Smi.
      Label if_indexissmi(assembler, Label::kDeferred),
          if_indexisnotsmi(assembler, Label::kDeferred);
      assembler->Branch(assembler->WordIsSmi(index), &if_indexissmi,
                        &if_indexisnotsmi);

      assembler->Bind(&if_indexissmi);
      {
        var_position.Bind(index);
        assembler->Goto(&if_positionissmi);
      }

      assembler->Bind(&if_indexisnotsmi);
      {
        // The ToIntegerStub canonicalizes everything in Smi range to Smi
        // representation, so any HeapNumber returned is not in Smi range.
        // The only exception here is -0.0, which we treat as 0.
        Node* index_value = assembler->LoadHeapNumberValue(index);
        Label if_indexiszero(assembler, Label::kDeferred),
            if_indexisnotzero(assembler, Label::kDeferred);
        assembler->Branch(assembler->Float64Equal(
                              index_value, assembler->Float64Constant(0.0)),
                          &if_indexiszero, &if_indexisnotzero);

        assembler->Bind(&if_indexiszero);
        {
          var_position.Bind(assembler->SmiConstant(Smi::FromInt(0)));
          assembler->Goto(&if_positionissmi);
        }

        assembler->Bind(&if_indexisnotzero);
        {
          // The {index} is some other integral Number, that is definitely
          // neither -0.0 nor in Smi range.
          assembler->Return(assembler->EmptyStringConstant());
        }
      }
    }
    assembler->Bind(&if_positionissmi);
    position = var_position.value();

    // Determine the actual length of the {receiver} String.
    Node* receiver_length =
        assembler->LoadObjectField(receiver, String::kLengthOffset);

    // Return "" if the Smi {position} is outside the bounds of the {receiver}.
    Label if_positioninbounds(assembler),
        if_positionnotinbounds(assembler, Label::kDeferred);
    assembler->Branch(assembler->SmiAboveOrEqual(position, receiver_length),
                      &if_positionnotinbounds, &if_positioninbounds);
    assembler->Bind(&if_positionnotinbounds);
    assembler->Return(assembler->EmptyStringConstant());
    assembler->Bind(&if_positioninbounds);
  }

  // Load the character code at the {position} from the {receiver}.
  Node* code = assembler->StringCharCodeAt(receiver, position);

  // And return the single character string with only that {code}.
  Node* result = assembler->StringFromCharCode(code);
  assembler->Return(result);
}

// ES6 section 21.1.3.2 String.prototype.charCodeAt ( pos )
void Builtins::Generate_StringPrototypeCharCodeAt(
    CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* receiver = assembler->Parameter(0);
  Node* position = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);

  // Check that {receiver} is coercible to Object and convert it to a String.
  receiver =
      assembler->ToThisString(context, receiver, "String.prototype.charCodeAt");

  // Convert the {position} to a Smi and check that it's in bounds of the
  // {receiver}.
  // TODO(bmeurer): Find an abstraction for this!
  {
    // Check if the {position} is already a Smi.
    Variable var_position(assembler, MachineRepresentation::kTagged);
    var_position.Bind(position);
    Label if_positionissmi(assembler),
        if_positionisnotsmi(assembler, Label::kDeferred);
    assembler->Branch(assembler->WordIsSmi(position), &if_positionissmi,
                      &if_positionisnotsmi);
    assembler->Bind(&if_positionisnotsmi);
    {
      // Convert the {position} to an Integer via the ToIntegerStub.
      Callable callable = CodeFactory::ToInteger(assembler->isolate());
      Node* index = assembler->CallStub(callable, context, position);

      // Check if the resulting {index} is now a Smi.
      Label if_indexissmi(assembler, Label::kDeferred),
          if_indexisnotsmi(assembler, Label::kDeferred);
      assembler->Branch(assembler->WordIsSmi(index), &if_indexissmi,
                        &if_indexisnotsmi);

      assembler->Bind(&if_indexissmi);
      {
        var_position.Bind(index);
        assembler->Goto(&if_positionissmi);
      }

      assembler->Bind(&if_indexisnotsmi);
      {
        // The ToIntegerStub canonicalizes everything in Smi range to Smi
        // representation, so any HeapNumber returned is not in Smi range.
        // The only exception here is -0.0, which we treat as 0.
        Node* index_value = assembler->LoadHeapNumberValue(index);
        Label if_indexiszero(assembler, Label::kDeferred),
            if_indexisnotzero(assembler, Label::kDeferred);
        assembler->Branch(assembler->Float64Equal(
                              index_value, assembler->Float64Constant(0.0)),
                          &if_indexiszero, &if_indexisnotzero);

        assembler->Bind(&if_indexiszero);
        {
          var_position.Bind(assembler->SmiConstant(Smi::FromInt(0)));
          assembler->Goto(&if_positionissmi);
        }

        assembler->Bind(&if_indexisnotzero);
        {
          // The {index} is some other integral Number, that is definitely
          // neither -0.0 nor in Smi range.
          assembler->Return(assembler->NaNConstant());
        }
      }
    }
    assembler->Bind(&if_positionissmi);
    position = var_position.value();

    // Determine the actual length of the {receiver} String.
    Node* receiver_length =
        assembler->LoadObjectField(receiver, String::kLengthOffset);

    // Return NaN if the Smi {position} is outside the bounds of the {receiver}.
    Label if_positioninbounds(assembler),
        if_positionnotinbounds(assembler, Label::kDeferred);
    assembler->Branch(assembler->SmiAboveOrEqual(position, receiver_length),
                      &if_positionnotinbounds, &if_positioninbounds);
    assembler->Bind(&if_positionnotinbounds);
    assembler->Return(assembler->NaNConstant());
    assembler->Bind(&if_positioninbounds);
  }

  // Load the character at the {position} from the {receiver}.
  Node* value = assembler->StringCharCodeAt(receiver, position);
  Node* result = assembler->SmiFromWord32(value);
  assembler->Return(result);
}

// ES6 section 21.1.3.25 String.prototype.toString ()
void Builtins::Generate_StringPrototypeToString(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* result = assembler->ToThisValue(
      context, receiver, PrimitiveType::kString, "String.prototype.toString");
  assembler->Return(result);
}

// ES6 section 21.1.3.27 String.prototype.trim ()
BUILTIN(StringPrototypeTrim) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.trim");
  return *String::Trim(string, String::kTrim);
}

// Non-standard WebKit extension
BUILTIN(StringPrototypeTrimLeft) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.trimLeft");
  return *String::Trim(string, String::kTrimLeft);
}

// Non-standard WebKit extension
BUILTIN(StringPrototypeTrimRight) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.trimRight");
  return *String::Trim(string, String::kTrimRight);
}

// ES6 section 21.1.3.28 String.prototype.valueOf ( )
void Builtins::Generate_StringPrototypeValueOf(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* result = assembler->ToThisValue(
      context, receiver, PrimitiveType::kString, "String.prototype.valueOf");
  assembler->Return(result);
}

}  // namespace internal
}  // namespace v8
