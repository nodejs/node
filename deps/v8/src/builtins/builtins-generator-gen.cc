// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

class GeneratorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit GeneratorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void GeneratorPrototypeResume(Node* receiver, Node* value, Node* context,
                                JSGeneratorObject::ResumeMode resume_mode,
                                char const* const method_name);
};

void GeneratorBuiltinsAssembler::GeneratorPrototypeResume(
    Node* receiver, Node* value, Node* context,
    JSGeneratorObject::ResumeMode resume_mode, char const* const method_name) {
  Node* closed = SmiConstant(JSGeneratorObject::kGeneratorClosed);

  // Check if the {receiver} is actually a JSGeneratorObject.
  Label if_receiverisincompatible(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &if_receiverisincompatible);
  Node* receiver_instance_type = LoadInstanceType(receiver);
  GotoIfNot(Word32Equal(receiver_instance_type,
                        Int32Constant(JS_GENERATOR_OBJECT_TYPE)),
            &if_receiverisincompatible);

  // Check if the {receiver} is running or already closed.
  Node* receiver_continuation =
      LoadObjectField(receiver, JSGeneratorObject::kContinuationOffset);
  Label if_receiverisclosed(this, Label::kDeferred),
      if_receiverisrunning(this, Label::kDeferred);
  GotoIf(SmiEqual(receiver_continuation, closed), &if_receiverisclosed);
  DCHECK_LT(JSGeneratorObject::kGeneratorExecuting,
            JSGeneratorObject::kGeneratorClosed);
  GotoIf(SmiLessThan(receiver_continuation, closed), &if_receiverisrunning);

  // Resume the {receiver} using our trampoline.
  Node* result =
      CallStub(CodeFactory::ResumeGenerator(isolate()), context, value,
               receiver, SmiConstant(resume_mode),
               SmiConstant(static_cast<int>(SuspendFlags::kGeneratorYield)));
  Return(result);

  BIND(&if_receiverisincompatible);
  {
    // The {receiver} is not a valid JSGeneratorObject.
    CallRuntime(Runtime::kThrowIncompatibleMethodReceiver, context,
                HeapConstant(
                    factory()->NewStringFromAsciiChecked(method_name, TENURED)),
                receiver);
    Unreachable();
  }

  BIND(&if_receiverisclosed);
  {
    Callable create_iter_result_object =
        CodeFactory::CreateIterResultObject(isolate());

    // The {receiver} is closed already.
    Node* result = nullptr;
    switch (resume_mode) {
      case JSGeneratorObject::kNext:
        result = CallStub(create_iter_result_object, context,
                          UndefinedConstant(), TrueConstant());
        break;
      case JSGeneratorObject::kReturn:
        result =
            CallStub(create_iter_result_object, context, value, TrueConstant());
        break;
      case JSGeneratorObject::kThrow:
        result = CallRuntime(Runtime::kThrow, context, value);
        break;
    }
    Return(result);
  }

  BIND(&if_receiverisrunning);
  {
    CallRuntime(Runtime::kThrowGeneratorRunning, context);
    Unreachable();
  }
}

// ES6 #sec-generator.prototype.next
TF_BUILTIN(GeneratorPrototypeNext, GeneratorBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  GeneratorPrototypeResume(receiver, value, context, JSGeneratorObject::kNext,
                           "[Generator].prototype.next");
}

// ES6 #sec-generator.prototype.return
TF_BUILTIN(GeneratorPrototypeReturn, GeneratorBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  GeneratorPrototypeResume(receiver, value, context, JSGeneratorObject::kReturn,
                           "[Generator].prototype.return");
}

// ES6 #sec-generator.prototype.throw
TF_BUILTIN(GeneratorPrototypeThrow, GeneratorBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* exception = Parameter(Descriptor::kException);
  Node* context = Parameter(Descriptor::kContext);
  GeneratorPrototypeResume(receiver, exception, context,
                           JSGeneratorObject::kThrow,
                           "[Generator].prototype.throw");
}

}  // namespace internal
}  // namespace v8
