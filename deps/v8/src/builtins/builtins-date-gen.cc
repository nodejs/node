// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 20.3 Date Objects

class DateBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit DateBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_DatePrototype_GetField(Node* context, Node* receiver,
                                       int field_index);
};

void DateBuiltinsAssembler::Generate_DatePrototype_GetField(Node* context,
                                                            Node* receiver,
                                                            int field_index) {
  Label receiver_not_date(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &receiver_not_date);
  Node* receiver_instance_type = LoadInstanceType(receiver);
  GotoIfNot(InstanceTypeEqual(receiver_instance_type, JS_DATE_TYPE),
            &receiver_not_date);

  // Load the specified date field, falling back to the runtime as necessary.
  if (field_index == JSDate::kDateValue) {
    Return(LoadObjectField(receiver, JSDate::kValueOffset));
  } else {
    if (field_index < JSDate::kFirstUncachedField) {
      Label stamp_mismatch(this, Label::kDeferred);
      Node* date_cache_stamp = Load(
          MachineType::AnyTagged(),
          ExternalConstant(ExternalReference::date_cache_stamp(isolate())));

      Node* cache_stamp = LoadObjectField(receiver, JSDate::kCacheStampOffset);
      GotoIf(WordNotEqual(date_cache_stamp, cache_stamp), &stamp_mismatch);
      Return(LoadObjectField(
          receiver, JSDate::kValueOffset + field_index * kPointerSize));

      BIND(&stamp_mismatch);
    }

    Node* field_index_smi = SmiConstant(field_index);
    Node* function =
        ExternalConstant(ExternalReference::get_date_field_function(isolate()));
    Node* result = CallCFunction2(
        MachineType::AnyTagged(), MachineType::AnyTagged(),
        MachineType::AnyTagged(), function, receiver, field_index_smi);
    Return(result);
  }

  // Raise a TypeError if the receiver is not a date.
  BIND(&receiver_not_date);
  { ThrowTypeError(context, MessageTemplate::kNotDateObject); }
}

TF_BUILTIN(DatePrototypeGetDate, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDay);
}

TF_BUILTIN(DatePrototypeGetDay, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekday);
}

TF_BUILTIN(DatePrototypeGetFullYear, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYear);
}

TF_BUILTIN(DatePrototypeGetHours, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHour);
}

TF_BUILTIN(DatePrototypeGetMilliseconds, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecond);
}

TF_BUILTIN(DatePrototypeGetMinutes, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinute);
}

TF_BUILTIN(DatePrototypeGetMonth, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonth);
}

TF_BUILTIN(DatePrototypeGetSeconds, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecond);
}

TF_BUILTIN(DatePrototypeGetTime, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDateValue);
}

TF_BUILTIN(DatePrototypeGetTimezoneOffset, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kTimezoneOffset);
}

TF_BUILTIN(DatePrototypeGetUTCDate, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDayUTC);
}

TF_BUILTIN(DatePrototypeGetUTCDay, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekdayUTC);
}

TF_BUILTIN(DatePrototypeGetUTCFullYear, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYearUTC);
}

TF_BUILTIN(DatePrototypeGetUTCHours, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHourUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMilliseconds, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecondUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMinutes, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinuteUTC);
}

TF_BUILTIN(DatePrototypeGetUTCMonth, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonthUTC);
}

TF_BUILTIN(DatePrototypeGetUTCSeconds, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecondUTC);
}

TF_BUILTIN(DatePrototypeValueOf, DateBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDateValue);
}

TF_BUILTIN(DatePrototypeToPrimitive, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* hint = Parameter(Descriptor::kHint);

  // Check if the {receiver} is actually a JSReceiver.
  Label receiver_is_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &receiver_is_invalid);
  GotoIfNot(IsJSReceiver(receiver), &receiver_is_invalid);

  // Dispatch to the appropriate OrdinaryToPrimitive builtin.
  Label hint_is_number(this), hint_is_string(this),
      hint_is_invalid(this, Label::kDeferred);

  // Fast cases for internalized strings.
  Node* number_string = LoadRoot(Heap::knumber_stringRootIndex);
  GotoIf(WordEqual(hint, number_string), &hint_is_number);
  Node* default_string = LoadRoot(Heap::kdefault_stringRootIndex);
  GotoIf(WordEqual(hint, default_string), &hint_is_string);
  Node* string_string = LoadRoot(Heap::kstring_stringRootIndex);
  GotoIf(WordEqual(hint, string_string), &hint_is_string);

  // Slow-case with actual string comparisons.
  GotoIf(TaggedIsSmi(hint), &hint_is_invalid);
  GotoIfNot(IsString(hint), &hint_is_invalid);
  GotoIf(WordEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, number_string),
             TrueConstant()),
         &hint_is_number);
  GotoIf(WordEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, default_string),
             TrueConstant()),
         &hint_is_string);
  GotoIf(WordEqual(
             CallBuiltin(Builtins::kStringEqual, context, hint, string_string),
             TrueConstant()),
         &hint_is_string);
  Goto(&hint_is_invalid);

  // Use the OrdinaryToPrimitive builtin to convert to a Number.
  BIND(&hint_is_number);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), OrdinaryToPrimitiveHint::kNumber);
    Node* result = CallStub(callable, context, receiver);
    Return(result);
  }

  // Use the OrdinaryToPrimitive builtin to convert to a String.
  BIND(&hint_is_string);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), OrdinaryToPrimitiveHint::kString);
    Node* result = CallStub(callable, context, receiver);
    Return(result);
  }

  // Raise a TypeError if the {hint} is invalid.
  BIND(&hint_is_invalid);
  { ThrowTypeError(context, MessageTemplate::kInvalidHint, hint); }

  // Raise a TypeError if the {receiver} is not a JSReceiver instance.
  BIND(&receiver_is_invalid);
  {
    ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                   StringConstant("Date.prototype [ @@toPrimitive ]"),
                   receiver);
  }
}

}  // namespace internal
}  // namespace v8
