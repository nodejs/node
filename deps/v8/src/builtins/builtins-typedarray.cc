// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

class TypedArrayBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit TypedArrayBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void GenerateTypedArrayPrototypeGetter(const char* method_name,
                                         int object_offset);
  template <IterationKind kIterationKind>
  void GenerateTypedArrayPrototypeIterationMethod(const char* method_name);
};

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

// ES6 section 22.2.3.1 get %TypedArray%.prototype.buffer
BUILTIN(TypedArrayPrototypeBuffer) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSTypedArray, typed_array, "get TypedArray.prototype.buffer");
  return *typed_array->GetBuffer();
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeGetter(
    const char* method_name, int object_offset) {
  Node* receiver = Parameter(0);
  Node* context = Parameter(3);

  // Check if the {receiver} is actually a JSTypedArray.
  Label receiver_is_incompatible(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &receiver_is_incompatible);
  GotoIfNot(HasInstanceType(receiver, JS_TYPED_ARRAY_TYPE),
            &receiver_is_incompatible);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(this, Label::kDeferred);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_receiverisneutered);
  Return(LoadObjectField(receiver, object_offset));

  Bind(&if_receiverisneutered);
  {
    // The {receiver}s buffer was neutered, default to zero.
    Return(SmiConstant(0));
  }

  Bind(&receiver_is_incompatible);
  {
    // The {receiver} is not a valid JSTypedArray.
    CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
                HeapConstant(
                    factory()->NewStringFromAsciiChecked(method_name, TENURED)),
                receiver);
    Unreachable();
  }
}

// ES6 section 22.2.3.2 get %TypedArray%.prototype.byteLength
TF_BUILTIN(TypedArrayPrototypeByteLength, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeGetter("get TypedArray.prototype.byteLength",
                                    JSTypedArray::kByteLengthOffset);
}

// ES6 section 22.2.3.3 get %TypedArray%.prototype.byteOffset
TF_BUILTIN(TypedArrayPrototypeByteOffset, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeGetter("get TypedArray.prototype.byteOffset",
                                    JSTypedArray::kByteOffsetOffset);
}

// ES6 section 22.2.3.18 get %TypedArray%.prototype.length
TF_BUILTIN(TypedArrayPrototypeLength, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeGetter("get TypedArray.prototype.length",
                                    JSTypedArray::kLengthOffset);
}

template <IterationKind kIterationKind>
void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeIterationMethod(
    const char* method_name) {
  Node* receiver = Parameter(0);
  Node* context = Parameter(3);

  Label throw_bad_receiver(this, Label::kDeferred);
  Label throw_typeerror(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &throw_bad_receiver);

  Node* map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(Word32NotEqual(instance_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
         &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(this, Label::kDeferred);
  GotoIf(IsDetachedBuffer(receiver_buffer), &if_receiverisneutered);

  Return(CreateArrayIterator(receiver, map, instance_type, context,
                             kIterationKind));

  Variable var_message(this, MachineRepresentation::kTagged);
  Bind(&throw_bad_receiver);
  var_message.Bind(SmiConstant(MessageTemplate::kNotTypedArray));
  Goto(&throw_typeerror);

  Bind(&if_receiverisneutered);
  var_message.Bind(
      SmiConstant(Smi::FromInt(MessageTemplate::kDetachedOperation)));
  Goto(&throw_typeerror);

  Bind(&throw_typeerror);
  {
    Node* method_arg = HeapConstant(
        isolate()->factory()->NewStringFromAsciiChecked(method_name, TENURED));
    Node* result = CallRuntime(Runtime::kThrowTypeError, context,
                               var_message.value(), method_arg);
    Return(result);
  }
}

TF_BUILTIN(TypedArrayPrototypeValues, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeIterationMethod<IterationKind::kValues>(
      "%TypedArray%.prototype.values()");
}

TF_BUILTIN(TypedArrayPrototypeEntries, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeIterationMethod<IterationKind::kEntries>(
      "%TypedArray%.prototype.entries()");
}

TF_BUILTIN(TypedArrayPrototypeKeys, TypedArrayBuiltinsAssembler) {
  GenerateTypedArrayPrototypeIterationMethod<IterationKind::kKeys>(
      "%TypedArray%.prototype.keys()");
}

namespace {

int64_t CapRelativeIndex(Handle<Object> num, int64_t minimum, int64_t maximum) {
  int64_t relative;
  if (V8_LIKELY(num->IsSmi())) {
    relative = Smi::cast(*num)->value();
  } else {
    DCHECK(num->IsHeapNumber());
    double fp = HeapNumber::cast(*num)->value();
    if (V8_UNLIKELY(!std::isfinite(fp))) {
      // +Infinity / -Infinity
      DCHECK(!std::isnan(fp));
      return fp < 0 ? minimum : maximum;
    }
    relative = static_cast<int64_t>(fp);
  }
  return relative < 0 ? std::max<int64_t>(relative + maximum, minimum)
                      : std::min<int64_t>(relative, maximum);
}

}  // namespace

BUILTIN(TypedArrayPrototypeCopyWithin) {
  HandleScope scope(isolate);

  Handle<JSTypedArray> array;
  const char* method = "%TypedArray%.prototype.copyWithin";
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, array, JSTypedArray::Validate(isolate, args.receiver(), method));

  if (V8_UNLIKELY(array->WasNeutered())) return *array;

  int64_t len = array->length_value();
  int64_t to = 0;
  int64_t from = 0;
  int64_t final = len;

  if (V8_LIKELY(args.length() > 1)) {
    Handle<Object> num;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num, Object::ToInteger(isolate, args.at<Object>(1)));
    to = CapRelativeIndex(num, 0, len);

    if (args.length() > 2) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, num, Object::ToInteger(isolate, args.at<Object>(2)));
      from = CapRelativeIndex(num, 0, len);

      Handle<Object> end = args.atOrUndefined(isolate, 3);
      if (!end->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, num,
                                           Object::ToInteger(isolate, end));
        final = CapRelativeIndex(num, 0, len);
      }
    }
  }

  int64_t count = std::min<int64_t>(final - from, len - to);
  if (count <= 0) return *array;

  // TypedArray buffer may have been transferred/detached during parameter
  // processing above. Return early in this case, to prevent potential UAF error
  // TODO(caitp): throw here, as though the full algorithm were performed (the
  // throw would have come from ecma262/#sec-integerindexedelementget)
  // (see )
  if (V8_UNLIKELY(array->WasNeutered())) return *array;

  // Ensure processed indexes are within array bounds
  DCHECK_GE(from, 0);
  DCHECK_LT(from, len);
  DCHECK_GE(to, 0);
  DCHECK_LT(to, len);
  DCHECK_GE(len - count, 0);

  Handle<FixedTypedArrayBase> elements(
      FixedTypedArrayBase::cast(array->elements()));
  size_t element_size = array->element_size();
  to = to * element_size;
  from = from * element_size;
  count = count * element_size;

  uint8_t* data = static_cast<uint8_t*>(elements->DataPtr());
  std::memmove(data + to, data + from, count);

  return *array;
}

}  // namespace internal
}  // namespace v8
