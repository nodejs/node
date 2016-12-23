// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/debug/debug.h"
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
  CHECK(IsResumableFunction(function->shared()->kind()));

  Handle<FixedArray> operand_stack;
  if (function->shared()->HasBytecodeArray()) {
    // New-style generators.
    DCHECK(!function->shared()->HasBaselineCode());
    int size = function->shared()->bytecode_array()->register_count();
    operand_stack = isolate->factory()->NewFixedArray(size);
  } else {
    // Old-style generators.
    DCHECK(function->shared()->HasBaselineCode());
    operand_stack = isolate->factory()->empty_fixed_array();
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
  CHECK(IsResumableFunction(frame->function()->shared()->kind()));
  DCHECK_EQ(frame->function(), generator_object->function());
  DCHECK(frame->function()->shared()->is_compiled());
  DCHECK(!frame->function()->IsOptimized());

  isolate->debug()->RecordAsyncFunction(generator_object);

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

RUNTIME_FUNCTION(Runtime_GeneratorGetFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->function();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetReceiver) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->receiver();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetInputOrDebugPos) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->input_or_debug_pos();
}

RUNTIME_FUNCTION(Runtime_GeneratorGetResumeMode) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->resume_mode());
}

RUNTIME_FUNCTION(Runtime_GeneratorGetContinuation) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return Smi::FromInt(generator->continuation());
}

RUNTIME_FUNCTION(Runtime_GeneratorGetSourcePosition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  if (!generator->is_suspended()) return isolate->heap()->undefined_value();
  return Smi::FromInt(generator->source_position());
}

}  // namespace internal
}  // namespace v8
