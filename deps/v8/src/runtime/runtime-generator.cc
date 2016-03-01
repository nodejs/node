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
  DCHECK(args.length() == 0);

  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  Handle<JSFunction> function(frame->function());
  RUNTIME_ASSERT(function->shared()->is_generator());

  Handle<JSGeneratorObject> generator;
  if (frame->IsConstructor()) {
    generator = handle(JSGeneratorObject::cast(frame->receiver()));
  } else {
    generator = isolate->factory()->NewJSGeneratorObject(function);
  }
  generator->set_function(*function);
  generator->set_context(Context::cast(frame->context()));
  generator->set_receiver(frame->receiver());
  generator->set_continuation(0);
  generator->set_operand_stack(isolate->heap()->empty_fixed_array());

  return *generator;
}


RUNTIME_FUNCTION(Runtime_SuspendJSGeneratorObject) {
  HandleScope handle_scope(isolate);
  DCHECK(args.length() == 1 || args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator_object, 0);

  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();
  RUNTIME_ASSERT(frame->function()->shared()->is_generator());
  DCHECK_EQ(frame->function(), generator_object->function());

  // The caller should have saved the context and continuation already.
  DCHECK_EQ(generator_object->context(), Context::cast(frame->context()));
  DCHECK_LT(0, generator_object->continuation());

  // We expect there to be at least two values on the operand stack: the return
  // value of the yield expression, and the arguments to this runtime call.
  // Neither of those should be saved.
  int operands_count = frame->ComputeOperandsCount();
  DCHECK_GE(operands_count, 1 + args.length());
  operands_count -= 1 + args.length();

  // Second argument indicates that we need to patch the handler table because
  // a delegating yield introduced a try-catch statement at expression level,
  // hence the operand count was off when we statically computed it.
  // TODO(mstarzinger): This special case disappears with do-expressions.
  if (args.length() == 2) {
    CONVERT_SMI_ARG_CHECKED(handler_index, 1);
    Handle<Code> code(frame->unchecked_code());
    Handle<HandlerTable> table(HandlerTable::cast(code->handler_table()));
    int handler_depth = operands_count - TryBlockConstant::kElementCount;
    table->SetRangeDepth(handler_index, handler_depth);
  }

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


// Note that this function is the slow path for resuming generators.  It is only
// called if the suspended activation had operands on the stack, stack handlers
// needing rewinding, or if the resume should throw an exception.  The fast path
// is handled directly in FullCodeGenerator::EmitGeneratorResume(), which is
// inlined into GeneratorNext and GeneratorThrow.  EmitGeneratorResumeResume is
// called in any case, as it needs to reconstruct the stack frame and make space
// for arguments and operands.
RUNTIME_FUNCTION(Runtime_ResumeJSGeneratorObject) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_CHECKED(JSGeneratorObject, generator_object, 0);
  CONVERT_ARG_CHECKED(Object, value, 1);
  CONVERT_SMI_ARG_CHECKED(resume_mode_int, 2);
  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();

  DCHECK_EQ(frame->function(), generator_object->function());
  DCHECK(frame->function()->is_compiled());

  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);

  Address pc = generator_object->function()->code()->instruction_start();
  int offset = generator_object->continuation();
  DCHECK(offset > 0);
  frame->set_pc(pc + offset);
  if (FLAG_enable_embedded_constant_pool) {
    frame->set_constant_pool(
        generator_object->function()->code()->constant_pool());
  }
  generator_object->set_continuation(JSGeneratorObject::kGeneratorExecuting);

  FixedArray* operand_stack = generator_object->operand_stack();
  int operands_count = operand_stack->length();
  if (operands_count != 0) {
    frame->RestoreOperandStack(operand_stack);
    generator_object->set_operand_stack(isolate->heap()->empty_fixed_array());
  }

  JSGeneratorObject::ResumeMode resume_mode =
      static_cast<JSGeneratorObject::ResumeMode>(resume_mode_int);
  switch (resume_mode) {
    case JSGeneratorObject::NEXT:
      return value;
    case JSGeneratorObject::THROW:
      return isolate->Throw(value);
  }

  UNREACHABLE();
  return isolate->ThrowIllegalOperation();
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


// Returns context of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->context();
}


// Returns receiver of generator activation.
RUNTIME_FUNCTION(Runtime_GeneratorGetReceiver) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);

  return generator->receiver();
}


// Returns generator continuation as a PC offset, or the magic -1 or 0 values.
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

  if (generator->is_suspended()) {
    Handle<Code> code(generator->function()->code(), isolate);
    int offset = generator->continuation();

    RUNTIME_ASSERT(0 <= offset && offset < code->Size());
    Address pc = code->address() + offset;

    return Smi::FromInt(code->SourcePosition(pc));
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GeneratorNext) {
  UNREACHABLE();  // Optimization disabled in SetUpGenerators().
  return NULL;
}


RUNTIME_FUNCTION(Runtime_GeneratorThrow) {
  UNREACHABLE();  // Optimization disabled in SetUpGenerators().
  return NULL;
}
}  // namespace internal
}  // namespace v8
