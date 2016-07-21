// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateJSGeneratorObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  RUNTIME_ASSERT(function->shared()->is_resumable());

  Handle<FixedArray> operand_stack;
  if (FLAG_ignition && FLAG_ignition_generators) {
    int size = function->shared()->bytecode_array()->register_count();
    operand_stack = isolate->factory()->NewFixedArray(size);
  } else {
    DCHECK(!function->shared()->HasBytecodeArray());
    operand_stack = handle(isolate->heap()->empty_fixed_array());
  }

  Handle<JSGeneratorObject> generator =
      isolate->factory()->NewJSGeneratorObject(function);
  generator->set_function(*function);
  generator->set_context(isolate->context());
  generator->set_receiver(*receiver);
  generator->set_operand_stack(*operand_stack);
  generator->set_continuation(JSGeneratorObject::kGeneratorExecuting);
  return *generator;
}


RUNTIME_FUNCTION(Runtime_SuspendJSGeneratorObject) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator_object, 0);

  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();
  RUNTIME_ASSERT(frame->function()->shared()->is_resumable());
  DCHECK_EQ(frame->function(), generator_object->function());
  DCHECK(frame->function()->shared()->is_compiled());
  DCHECK(!frame->function()->IsOptimized());

  // The caller should have saved the context and continuation already.
  DCHECK_EQ(generator_object->context(), Context::cast(frame->context()));
  DCHECK_LT(0, generator_object->continuation());

  // We expect there to be at least two values on the operand stack: the return
  // value of the yield expression, and the arguments to this runtime call.
  // Neither of those should be saved.
  int operands_count = frame->ComputeOperandsCount();
  DCHECK_GE(operands_count, 1 + args.length());
  operands_count -= 1 + args.length();

  if (operands_count == 0) {
    // Although it's semantically harmless to call this function with an
    // operands_count of zero, it is also unnecessary.
    DCHECK_EQ(generator_object->operand_stack(),
              isolate->heap()->empty_fixed_array());
  } else {
    Handle<FixedArray> operand_stack =
        isolate->factory()->NewFixedArray(operands_count);
    frame->SaveOperandStack(*operand_stack);
    generator_object->set_operand_stack(*operand_stack);
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GeneratorClose) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  generator->set_continuation(JSGeneratorObject::kGeneratorClosed);

  return isolate->heap()->undefined_value();
}


// Returns function of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->function();
}


// Returns receiver of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetReceiver) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->receiver();
}


// Returns input of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetInput) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->input();
}


// Returns resume mode of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetResumeMode) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->resume_mode());
}


RUNTIME_FUNCTION(Runtime_GeneratorSetContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  generator->set_context(isolate->context());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GeneratorGetContinuation) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->continuation());
}


RUNTIME_FUNCTION(Runtime_GeneratorSetContinuation) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);
  CONVERT_SMI_ARG_CHECKED(continuation, 1);

  generator->set_continuation(continuation);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GeneratorLoadRegister) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);

  DCHECK(FLAG_ignition && FLAG_ignition_generators);
  DCHECK(generator->function()->shared()->HasBytecodeArray());

  return generator->operand_stack()->get(index);
}


RUNTIME_FUNCTION(Runtime_GeneratorStoreRegister) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  DCHECK(FLAG_ignition && FLAG_ignition_generators);
  DCHECK(generator->function()->shared()->HasBytecodeArray());

  generator->operand_stack()->set(index, *value);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GeneratorGetSourcePosition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  if (generator->is_suspended()) {
    Handle<Code> code(generator->function()->code(), isolate);
    int offset = generator->continuation();
    RUNTIME_ASSERT(0 <= offset && offset < code->instruction_size());
    return Smi::FromInt(code->SourcePosition(offset));
  }

  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
