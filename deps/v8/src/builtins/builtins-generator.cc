// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/code-factory.h"

namespace v8 {
namespace internal {

namespace {

void Generate_GeneratorPrototypeResume(
    CodeStubAssembler* assembler, JSGeneratorObject::ResumeMode resume_mode,
    char const* const method_name) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* value = assembler->Parameter(1);
  Node* context = assembler->Parameter(4);
  Node* closed =
      assembler->SmiConstant(Smi::FromInt(JSGeneratorObject::kGeneratorClosed));

  // Check if the {receiver} is actually a JSGeneratorObject.
  Label if_receiverisincompatible(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->WordIsSmi(receiver), &if_receiverisincompatible);
  Node* receiver_instance_type = assembler->LoadInstanceType(receiver);
  assembler->GotoUnless(assembler->Word32Equal(
                            receiver_instance_type,
                            assembler->Int32Constant(JS_GENERATOR_OBJECT_TYPE)),
                        &if_receiverisincompatible);

  // Check if the {receiver} is running or already closed.
  Node* receiver_continuation = assembler->LoadObjectField(
      receiver, JSGeneratorObject::kContinuationOffset);
  Label if_receiverisclosed(assembler, Label::kDeferred),
      if_receiverisrunning(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->SmiEqual(receiver_continuation, closed),
                    &if_receiverisclosed);
  DCHECK_LT(JSGeneratorObject::kGeneratorExecuting,
            JSGeneratorObject::kGeneratorClosed);
  assembler->GotoIf(assembler->SmiLessThan(receiver_continuation, closed),
                    &if_receiverisrunning);

  // Resume the {receiver} using our trampoline.
  Node* result = assembler->CallStub(
      CodeFactory::ResumeGenerator(assembler->isolate()), context, value,
      receiver, assembler->SmiConstant(Smi::FromInt(resume_mode)));
  assembler->Return(result);

  assembler->Bind(&if_receiverisincompatible);
  {
    // The {receiver} is not a valid JSGeneratorObject.
    Node* result = assembler->CallRuntime(
        Runtime::kThrowIncompatibleMethodReceiver, context,
        assembler->HeapConstant(assembler->factory()->NewStringFromAsciiChecked(
            method_name, TENURED)),
        receiver);
    assembler->Return(result);  // Never reached.
  }

  assembler->Bind(&if_receiverisclosed);
  {
    // The {receiver} is closed already.
    Node* result = nullptr;
    switch (resume_mode) {
      case JSGeneratorObject::kNext:
        result = assembler->CallRuntime(Runtime::kCreateIterResultObject,
                                        context, assembler->UndefinedConstant(),
                                        assembler->BooleanConstant(true));
        break;
      case JSGeneratorObject::kReturn:
        result =
            assembler->CallRuntime(Runtime::kCreateIterResultObject, context,
                                   value, assembler->BooleanConstant(true));
        break;
      case JSGeneratorObject::kThrow:
        result = assembler->CallRuntime(Runtime::kThrow, context, value);
        break;
    }
    assembler->Return(result);
  }

  assembler->Bind(&if_receiverisrunning);
  {
    Node* result =
        assembler->CallRuntime(Runtime::kThrowGeneratorRunning, context);
    assembler->Return(result);  // Never reached.
  }
}

}  // anonymous namespace

// ES6 section 25.3.1.2 Generator.prototype.next ( value )
void Builtins::Generate_GeneratorPrototypeNext(CodeStubAssembler* assembler) {
  Generate_GeneratorPrototypeResume(assembler, JSGeneratorObject::kNext,
                                    "[Generator].prototype.next");
}

// ES6 section 25.3.1.3 Generator.prototype.return ( value )
void Builtins::Generate_GeneratorPrototypeReturn(CodeStubAssembler* assembler) {
  Generate_GeneratorPrototypeResume(assembler, JSGeneratorObject::kReturn,
                                    "[Generator].prototype.return");
}

// ES6 section 25.3.1.4 Generator.prototype.throw ( exception )
void Builtins::Generate_GeneratorPrototypeThrow(CodeStubAssembler* assembler) {
  Generate_GeneratorPrototypeResume(assembler, JSGeneratorObject::kThrow,
                                    "[Generator].prototype.throw");
}

}  // namespace internal
}  // namespace v8
