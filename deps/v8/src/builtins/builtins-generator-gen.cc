// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/execution/isolate.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

class GeneratorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit GeneratorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void GeneratorPrototypeResume(CodeStubArguments* args, Node* receiver,
                                Node* value, Node* context,
                                JSGeneratorObject::ResumeMode resume_mode,
                                char const* const method_name);
};

void GeneratorBuiltinsAssembler::GeneratorPrototypeResume(
    CodeStubArguments* args, Node* receiver, Node* value, Node* context,
    JSGeneratorObject::ResumeMode resume_mode, char const* const method_name) {
  // Check if the {receiver} is actually a JSGeneratorObject.
  ThrowIfNotInstanceType(context, receiver, JS_GENERATOR_OBJECT_TYPE,
                         method_name);

  // Check if the {receiver} is running or already closed.
  TNode<Smi> receiver_continuation =
      CAST(LoadObjectField(receiver, JSGeneratorObject::kContinuationOffset));
  Label if_receiverisclosed(this, Label::kDeferred),
      if_receiverisrunning(this, Label::kDeferred);
  TNode<Smi> closed = SmiConstant(JSGeneratorObject::kGeneratorClosed);
  GotoIf(SmiEqual(receiver_continuation, closed), &if_receiverisclosed);
  DCHECK_LT(JSGeneratorObject::kGeneratorExecuting,
            JSGeneratorObject::kGeneratorClosed);
  GotoIf(SmiLessThan(receiver_continuation, closed), &if_receiverisrunning);

  // Remember the {resume_mode} for the {receiver}.
  StoreObjectFieldNoWriteBarrier(receiver, JSGeneratorObject::kResumeModeOffset,
                                 SmiConstant(resume_mode));

  // Resume the {receiver} using our trampoline.
  VARIABLE(var_exception, MachineRepresentation::kTagged, UndefinedConstant());
  Label if_exception(this, Label::kDeferred), if_final_return(this);
  Node* result = CallStub(CodeFactory::ResumeGenerator(isolate()), context,
                          value, receiver);
  // Make sure we close the generator if there was an exception.
  GotoIfException(result, &if_exception, &var_exception);

  // If the generator is not suspended (i.e., its state is 'executing'),
  // close it and wrap the return value in IteratorResult.
  TNode<Smi> result_continuation =
      CAST(LoadObjectField(receiver, JSGeneratorObject::kContinuationOffset));

  // The generator function should not close the generator by itself, let's
  // check it is indeed not closed yet.
  CSA_ASSERT(this, SmiNotEqual(result_continuation, closed));

  TNode<Smi> executing = SmiConstant(JSGeneratorObject::kGeneratorExecuting);
  GotoIf(SmiEqual(result_continuation, executing), &if_final_return);

  args->PopAndReturn(result);

  BIND(&if_final_return);
  {
    // Close the generator.
    StoreObjectFieldNoWriteBarrier(
        receiver, JSGeneratorObject::kContinuationOffset, closed);
    // Return the wrapped result.
    args->PopAndReturn(CallBuiltin(Builtins::kCreateIterResultObject, context,
                                   result, TrueConstant()));
  }

  BIND(&if_receiverisclosed);
  {
    // The {receiver} is closed already.
    Node* result = nullptr;
    switch (resume_mode) {
      case JSGeneratorObject::kNext:
        result = CallBuiltin(Builtins::kCreateIterResultObject, context,
                             UndefinedConstant(), TrueConstant());
        break;
      case JSGeneratorObject::kReturn:
        result = CallBuiltin(Builtins::kCreateIterResultObject, context, value,
                             TrueConstant());
        break;
      case JSGeneratorObject::kThrow:
        result = CallRuntime(Runtime::kThrow, context, value);
        break;
    }
    args->PopAndReturn(result);
  }

  BIND(&if_receiverisrunning);
  { ThrowTypeError(context, MessageTemplate::kGeneratorRunning); }

  BIND(&if_exception);
  {
    StoreObjectFieldNoWriteBarrier(
        receiver, JSGeneratorObject::kContinuationOffset, closed);
    CallRuntime(Runtime::kReThrow, context, var_exception.value());
    Unreachable();
  }
}

// ES6 #sec-generator.prototype.next
TF_BUILTIN(GeneratorPrototypeNext, GeneratorBuiltinsAssembler) {
  const int kValueArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* receiver = args.GetReceiver();
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(Descriptor::kContext);

  GeneratorPrototypeResume(&args, receiver, value, context,
                           JSGeneratorObject::kNext,
                           "[Generator].prototype.next");
}

// ES6 #sec-generator.prototype.return
TF_BUILTIN(GeneratorPrototypeReturn, GeneratorBuiltinsAssembler) {
  const int kValueArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* receiver = args.GetReceiver();
  Node* value = args.GetOptionalArgumentValue(kValueArg);
  Node* context = Parameter(Descriptor::kContext);

  GeneratorPrototypeResume(&args, receiver, value, context,
                           JSGeneratorObject::kReturn,
                           "[Generator].prototype.return");
}

// ES6 #sec-generator.prototype.throw
TF_BUILTIN(GeneratorPrototypeThrow, GeneratorBuiltinsAssembler) {
  const int kExceptionArg = 0;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* receiver = args.GetReceiver();
  Node* exception = args.GetOptionalArgumentValue(kExceptionArg);
  Node* context = Parameter(Descriptor::kContext);

  GeneratorPrototypeResume(&args, receiver, exception, context,
                           JSGeneratorObject::kThrow,
                           "[Generator].prototype.throw");
}

}  // namespace internal
}  // namespace v8
