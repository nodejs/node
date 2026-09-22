// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-inl.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/objects/dictionary.h"

namespace v8 {
namespace internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

// -----------------------------------------------------------------------------
// ES6 section 20.3 Date Objects

class DateBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit DateBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_IsDateCheck(TNode<Context> context, TNode<Object> receiver);
  void Generate_DatePrototype_GetField(TNode<Context> context,
                                       TNode<Object> receiver, int field_index);
};

void DateBuiltinsAssembler::Generate_IsDateCheck(TNode<Context> context,
                                                 TNode<Object> receiver) {
  Label ok(this), receiver_not_date(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &receiver_not_date);
  TNode<Uint16T> receiver_instance_type = LoadInstanceType(CAST(receiver));
  Branch(InstanceTypeEqual(receiver_instance_type, JS_DATE_TYPE), &ok,
         &receiver_not_date);

  // Raise a TypeError if the receiver is not a date.
  BIND(&receiver_not_date);
  { ThrowTypeError(context, MessageTemplate::kNotDateObject); }

  BIND(&ok);
}

void DateBuiltinsAssembler::Generate_DatePrototype_GetField(
    TNode<Context> context, TNode<Object> receiver, int field_index) {
  Generate_IsDateCheck(context, receiver);

  TNode<JSDate> date_receiver = CAST(receiver);
  // Load the specified date field, falling back to the runtime as necessary.
  if (field_index < JSDate::kFirstUncachedField) {
    Label stamp_mismatch(this, Label::kDeferred);
    TNode<Object> date_cache_stamp =
        Load<Object>(IsolateField(IsolateFieldId::kDateCacheStamp));

    TNode<Object> cache_stamp =
        LoadObjectField(date_receiver, offsetof(JSDate, cache_stamp_));
    GotoIf(TaggedNotEqual(date_cache_stamp, cache_stamp), &stamp_mismatch);
    Return(LoadObjectField(
        date_receiver, offsetof(JSDate, year_) + field_index * kTaggedSize));

    BIND(&stamp_mismatch);
  }

  TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  TNode<Smi> field_index_smi = SmiConstant(field_index);
  TNode<ExternalReference> function =
      ExternalConstant(ExternalReference::get_date_field_function());
  TNode<Object> result = CAST(
      CallCFunction(function, MachineType::AnyTagged(),
                    std::make_pair(MachineType::Pointer(), isolate_ptr),
                    std::make_pair(MachineType::AnyTagged(), date_receiver),
                    std::make_pair(MachineType::AnyTagged(), field_index_smi)));
  Return(result);
}

// https://tc39.es/ecma262/#sec-date.prototype.getdate
TF_BUILTIN(DatePrototypeGetDate, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDay);
}

// https://tc39.es/ecma262/#sec-date.prototype.getday
TF_BUILTIN(DatePrototypeGetDay, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekday);
}

// https://tc39.es/ecma262/#sec-date.prototype.getfullyear
TF_BUILTIN(DatePrototypeGetFullYear, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYear);
}

// https://tc39.es/ecma262/#sec-date.prototype.gethours
TF_BUILTIN(DatePrototypeGetHours, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHour);
}

// https://tc39.es/ecma262/#sec-date.prototype.getmilliseconds
TF_BUILTIN(DatePrototypeGetMilliseconds, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecond);
}

// https://tc39.es/ecma262/#sec-date.prototype.getminutes
TF_BUILTIN(DatePrototypeGetMinutes, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinute);
}

// https://tc39.es/ecma262/#sec-date.prototype.getmonth
TF_BUILTIN(DatePrototypeGetMonth, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonth);
}

// https://tc39.es/ecma262/#sec-date.prototype.getseconds
TF_BUILTIN(DatePrototypeGetSeconds, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecond);
}

// https://tc39.es/ecma262/#sec-date.prototype.gettime
TF_BUILTIN(DatePrototypeGetTime, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_IsDateCheck(context, receiver);
  TNode<JSDate> date_receiver = CAST(receiver);
  Return(ChangeFloat64ToTagged(
      LoadObjectField<Float64T>(date_receiver, offsetof(JSDate, value_))));
}

// https://tc39.es/ecma262/#sec-date.prototype.gettimezoneoffset
TF_BUILTIN(DatePrototypeGetTimezoneOffset, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kTimezoneOffset);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcdate
TF_BUILTIN(DatePrototypeGetUTCDate, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kDayUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcday
TF_BUILTIN(DatePrototypeGetUTCDay, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kWeekdayUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcfullyear
TF_BUILTIN(DatePrototypeGetUTCFullYear, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kYearUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutchours
TF_BUILTIN(DatePrototypeGetUTCHours, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kHourUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcmilliseconds
TF_BUILTIN(DatePrototypeGetUTCMilliseconds, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMillisecondUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcminutes
TF_BUILTIN(DatePrototypeGetUTCMinutes, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMinuteUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcmonth
TF_BUILTIN(DatePrototypeGetUTCMonth, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kMonthUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.getutcseconds
TF_BUILTIN(DatePrototypeGetUTCSeconds, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_DatePrototype_GetField(context, receiver, JSDate::kSecondUTC);
}

// https://tc39.es/ecma262/#sec-date.prototype.valueof
TF_BUILTIN(DatePrototypeValueOf, DateBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  Generate_IsDateCheck(context, receiver);
  TNode<JSDate> date_receiver = CAST(receiver);
  Return(ChangeFloat64ToTagged(
      LoadObjectField<Float64T>(date_receiver, offsetof(JSDate, value_))));
}

// https://tc39.es/ecma262/#sec-date.prototype-@@toprimitive
TF_BUILTIN(DatePrototypeToPrimitive, CodeStubAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto hint = Parameter<Object>(Descriptor::kHint);

  // Check if the {receiver} is actually a JSReceiver.
  Label receiver_is_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &receiver_is_invalid);
  GotoIfNot(JSAnyIsNotPrimitive(CAST(receiver)), &receiver_is_invalid);

  // Dispatch to the appropriate OrdinaryToPrimitive builtin.
  Label hint_is_number(this), hint_is_string(this),
      hint_is_invalid(this, Label::kDeferred);

  // Fast cases for internalized strings.
  TNode<String> number_string = NumberStringConstant();
  GotoIf(TaggedEqual(hint, number_string), &hint_is_number);
  TNode<String> default_string = DefaultStringConstant();
  GotoIf(TaggedEqual(hint, default_string), &hint_is_string);
  TNode<String> string_string = StringStringConstant();
  GotoIf(TaggedEqual(hint, string_string), &hint_is_string);

  // Slow-case with actual string comparisons.
  GotoIf(TaggedIsSmi(hint), &hint_is_invalid);
  GotoIfNot(IsString(CAST(hint)), &hint_is_invalid);

  TNode<IntPtrT> hint_length = LoadStringLengthAsWord(CAST(hint));
  GotoIfStringEqual(CAST(hint), hint_length, number_string, &hint_is_number);
  GotoIfStringEqual(CAST(hint), hint_length, default_string, &hint_is_string);
  GotoIfStringEqual(CAST(hint), hint_length, string_string, &hint_is_string);
  Goto(&hint_is_invalid);

  // Use the OrdinaryToPrimitive builtin to convert to a Number.
  BIND(&hint_is_number);
  {
    Builtin builtin =
        Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint::kNumber);
    TNode<Object> result = CallBuiltin(builtin, context, receiver);
    Return(result);
  }

  // Use the OrdinaryToPrimitive builtin to convert to a String.
  BIND(&hint_is_string);
  {
    Builtin builtin =
        Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint::kString);
    TNode<Object> result = CallBuiltin(builtin, context, receiver);
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

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace internal
}  // namespace v8
