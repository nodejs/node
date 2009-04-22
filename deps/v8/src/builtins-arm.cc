// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "codegen-inl.h"
#include "debug.h"
#include "runtime.h"

namespace v8 { namespace internal {


#define __ masm->


void Builtins::Generate_Adaptor(MacroAssembler* masm, CFunctionId id) {
  // TODO(1238487): Don't pass the function in a static variable.
  __ mov(ip, Operand(ExternalReference::builtin_passed_function()));
  __ str(r1, MemOperand(ip, 0));

  // The actual argument count has already been loaded into register
  // r0, but JumpToBuiltin expects r0 to contain the number of
  // arguments including the receiver.
  __ add(r0, r0, Operand(1));
  __ JumpToBuiltin(ExternalReference(id));
}


void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  __ EnterConstructFrame();

  // Preserve the two incoming parameters
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ push(r0);  // smi-tagged arguments count
  __ push(r1);  // constructor function

  // Allocate the new receiver object.
  __ push(r1);  // argument for Runtime_NewObject
  __ CallRuntime(Runtime::kNewObject, 1);
  __ push(r0);  // save the receiver

  // Push the function and the allocated receiver from the stack.
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(r1, MemOperand(sp, kPointerSize));
  __ push(r1);  // function
  __ push(r0);  // receiver

  // Reload the number of arguments from the stack.
  // r1: constructor function
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  __ ldr(r3, MemOperand(sp, 4 * kPointerSize));

  // Setup pointer to last argument.
  __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

  // Setup number of arguments for function call below
  __ mov(r0, Operand(r3, LSR, kSmiTagSize));

  // Copy arguments and receiver to the expression stack.
  // r0: number of arguments
  // r2: address of last argument (caller sp)
  // r1: constructor function
  // r3: number of arguments (smi-tagged)
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  Label loop, entry;
  __ b(&entry);
  __ bind(&loop);
  __ ldr(ip, MemOperand(r2, r3, LSL, kPointerSizeLog2 - 1));
  __ push(ip);
  __ bind(&entry);
  __ sub(r3, r3, Operand(2), SetCC);
  __ b(ge, &loop);

  // Call the function.
  // r0: number of arguments
  // r1: constructor function
  ParameterCount actual(r0);
  __ InvokeFunction(r1, actual, CALL_FUNCTION);

  // Pop the function from the stack.
  // sp[0]: constructor function
  // sp[2]: receiver
  // sp[3]: constructor function
  // sp[4]: number of arguments (smi-tagged)
  __ pop();

  // Restore context from the frame.
  // r0: result
  // sp[0]: receiver
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, exit;

  // If the result is a smi, it is *not* an object in the ECMA sense.
  // r0: result
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_OBJECT_TYPE, it is not an object in the ECMA sense.
  __ ldr(r3, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  __ cmp(r3, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(ge, &exit);

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ ldr(r0, MemOperand(sp));

  // Remove receiver from the stack, remove caller arguments, and
  // return.
  __ bind(&exit);
  // r0: result
  // sp[0]: receiver (newly allocated object)
  // sp[1]: constructor function
  // sp[2]: number of arguments (smi-tagged)
  __ ldr(r1, MemOperand(sp, 2 * kPointerSize));
  __ LeaveConstructFrame();
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - 1));
  __ add(sp, sp, Operand(kPointerSize));
  __ mov(pc, Operand(lr));
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r7, cp may be clobbered

  // Clear the context before we push it when entering the JS frame.
  __ mov(cp, Operand(0));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Setup the context from the function argument.
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Push the function and the receiver onto the stack.
  __ push(r1);
  __ push(r2);

  // Copy arguments to the stack in a loop.
  // r1: function
  // r3: argc
  // r4: argv, i.e. points to first arg
  Label loop, entry;
  __ add(r2, r4, Operand(r3, LSL, kPointerSizeLog2));
  // r2 points past last arg.
  __ b(&entry);
  __ bind(&loop);
  __ ldr(r0, MemOperand(r4, kPointerSize, PostIndex));  // read next parameter
  __ ldr(r0, MemOperand(r0));  // dereference handle
  __ push(r0);  // push parameter
  __ bind(&entry);
  __ cmp(r4, Operand(r2));
  __ b(ne, &loop);

  // Initialize all JavaScript callee-saved registers, since they will be seen
  // by the garbage collector as part of handlers.
  __ mov(r4, Operand(Factory::undefined_value()));
  __ mov(r5, Operand(r4));
  __ mov(r6, Operand(r4));
  __ mov(r7, Operand(r4));
  if (kR9Available == 1)
    __ mov(r9, Operand(r4));

  // Invoke the code and pass argc as r0.
  __ mov(r0, Operand(r3));
  if (is_construct) {
    __ Call(Handle<Code>(Builtins::builtin(Builtins::JSConstructCall)),
            RelocInfo::CODE_TARGET);
  } else {
    ParameterCount actual(r0);
    __ InvokeFunction(r1, actual, CALL_FUNCTION);
  }

  // Exit the JS frame and remove the parameters (except function), and return.
  // Respect ABI stack constraint.
  __ LeaveInternalFrame();
  __ mov(pc, lr);

  // r0: result
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of argument
  { Label done;
    __ tst(r0, Operand(r0));
    __ b(ne, &done);
    __ mov(r2, Operand(Factory::undefined_value()));
    __ push(r2);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call from the stack.
  // r0: actual number of argument
  { Label done, non_function, function;
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ tst(r1, Operand(kSmiTagMask));
    __ b(eq, &non_function);
    __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
    __ cmp(r2, Operand(JS_FUNCTION_TYPE));
    __ b(eq, &function);

    // Non-function called: Clear the function to force exception.
    __ bind(&non_function);
    __ mov(r1, Operand(0));
    __ b(&done);

    // Change the context eagerly because it will be used below to get the
    // right global object.
    __ bind(&function);
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    __ bind(&done);
  }

  // 3. Make sure first argument is an object; convert if necessary.
  // r0: actual number of arguments
  // r1: function
  { Label call_to_object, use_global_receiver, patch_receiver, done;
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ ldr(r2, MemOperand(r2, -kPointerSize));

    // r0: actual number of arguments
    // r1: function
    // r2: first argument
    __ tst(r2, Operand(kSmiTagMask));
    __ b(eq, &call_to_object);

    __ mov(r3, Operand(Factory::null_value()));
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);
    __ mov(r3, Operand(Factory::undefined_value()));
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);

    __ ldr(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
    __ ldrb(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
    __ cmp(r3, Operand(FIRST_JS_OBJECT_TYPE));
    __ b(lt, &call_to_object);
    __ cmp(r3, Operand(LAST_JS_OBJECT_TYPE));
    __ b(le, &done);

    __ bind(&call_to_object);
    __ EnterInternalFrame();

    // Store number of arguments and function across the call into the runtime.
    __ mov(r0, Operand(r0, LSL, kSmiTagSize));
    __ push(r0);
    __ push(r1);

    __ push(r2);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
    __ mov(r2, r0);

    // Restore number of arguments and function.
    __ pop(r1);
    __ pop(r0);
    __ mov(r0, Operand(r0, ASR, kSmiTagSize));

    __ LeaveInternalFrame();
    __ b(&patch_receiver);

    // Use the global receiver object from the called function as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ ldr(r2, FieldMemOperand(cp, kGlobalIndex));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ add(r3, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(r3, -kPointerSize));

    __ bind(&done);
  }

  // 4. Shift stuff one slot down the stack
  // r0: actual number of arguments (including call() receiver)
  // r1: function
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ add(r2, r2, Operand(kPointerSize));  // copy receiver too

    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, -kPointerSize));
    __ str(ip, MemOperand(r2));
    __ sub(r2, r2, Operand(kPointerSize));
    __ cmp(r2, sp);
    __ b(ne, &loop);
  }

  // 5. Adjust the actual number of arguments and remove the top element.
  // r0: actual number of arguments (including call() receiver)
  // r1: function
  __ sub(r0, r0, Operand(1));
  __ add(sp, sp, Operand(kPointerSize));

  // 6. Get the code for the function or the non-function builtin.
  //    If number of expected arguments matches, then call. Otherwise restart
  //    the arguments adaptor stub.
  // r0: actual number of arguments
  // r1: function
  { Label invoke;
    __ tst(r1, r1);
    __ b(ne, &invoke);
    __ mov(r2, Operand(0));  // expected arguments is 0 for CALL_NON_FUNCTION
    __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
    __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
                         RelocInfo::CODE_TARGET);

    __ bind(&invoke);
    __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r2,
           FieldMemOperand(r3,
                           SharedFunctionInfo::kFormalParameterCountOffset));
    __ ldr(r3,
           MemOperand(r3, SharedFunctionInfo::kCodeOffset - kHeapObjectTag));
    __ add(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ cmp(r2, r0);  // Check formal and actual parameter counts.
    __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
                         RelocInfo::CODE_TARGET, ne);

    // 7. Jump to the code in r3 without checking arguments.
    ParameterCount expected(0);
    __ InvokeCode(r3, expected, expected, JUMP_FUNCTION);
  }
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  const int kIndexOffset    = -5 * kPointerSize;
  const int kLimitOffset    = -4 * kPointerSize;
  const int kArgsOffset     =  2 * kPointerSize;
  const int kRecvOffset     =  3 * kPointerSize;
  const int kFunctionOffset =  4 * kPointerSize;

  __ EnterInternalFrame();

  __ ldr(r0, MemOperand(fp, kFunctionOffset));  // get the function
  __ push(r0);
  __ ldr(r0, MemOperand(fp, kArgsOffset));  // get the args array
  __ push(r0);
  __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_JS);

  // Eagerly check for stack-overflow before starting to push the arguments.
  // r0: number of arguments
  Label okay;
  ExternalReference stack_guard_limit_address =
      ExternalReference::address_of_stack_guard_limit();
  __ mov(r2, Operand(stack_guard_limit_address));
  __ ldr(r2, MemOperand(r2));
  __ sub(r2, sp, r2);
  __ sub(r2, r2, Operand(3 * kPointerSize));  // limit, index, receiver

  __ cmp(r2, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ b(hi, &okay);

  // Out of stack space.
  __ ldr(r1, MemOperand(fp, kFunctionOffset));
  __ push(r1);
  __ push(r0);
  __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_JS);

  // Push current limit and index.
  __ bind(&okay);
  __ push(r0);  // limit
  __ mov(r1, Operand(0));  // initial index
  __ push(r1);

  // Change context eagerly to get the right global object if necessary.
  __ ldr(r0, MemOperand(fp, kFunctionOffset));
  __ ldr(cp, FieldMemOperand(r0, JSFunction::kContextOffset));

  // Compute the receiver.
  Label call_to_object, use_global_receiver, push_receiver;
  __ ldr(r0, MemOperand(fp, kRecvOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &call_to_object);
  __ mov(r1, Operand(Factory::null_value()));
  __ cmp(r0, r1);
  __ b(eq, &use_global_receiver);
  __ mov(r1, Operand(Factory::undefined_value()));
  __ cmp(r0, r1);
  __ b(eq, &use_global_receiver);

  // Check if the receiver is already a JavaScript object.
  // r0: receiver
  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r1, Map::kInstanceTypeOffset));
  __ cmp(r1, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, &call_to_object);
  __ cmp(r1, Operand(LAST_JS_OBJECT_TYPE));
  __ b(le, &push_receiver);

  // Convert the receiver to a regular object.
  // r0: receiver
  __ bind(&call_to_object);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
  __ b(&push_receiver);

  // Use the current global receiver object as the receiver.
  __ bind(&use_global_receiver);
  const int kGlobalOffset =
      Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  __ ldr(r0, FieldMemOperand(cp, kGlobalOffset));
  __ ldr(r0, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));

  // Push the receiver.
  // r0: receiver
  __ bind(&push_receiver);
  __ push(r0);

  // Copy all arguments from the array to the stack.
  Label entry, loop;
  __ ldr(r0, MemOperand(fp, kIndexOffset));
  __ b(&entry);

  // Load the current argument from the arguments array and push it to the
  // stack.
  // r0: current argument index
  __ bind(&loop);
  __ ldr(r1, MemOperand(fp, kArgsOffset));
  __ push(r1);
  __ push(r0);

  // Call the runtime to access the property in the arguments array.
  __ CallRuntime(Runtime::kGetProperty, 2);
  __ push(r0);

  // Use inline caching to access the arguments.
  __ ldr(r0, MemOperand(fp, kIndexOffset));
  __ add(r0, r0, Operand(1 << kSmiTagSize));
  __ str(r0, MemOperand(fp, kIndexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ bind(&entry);
  __ ldr(r1, MemOperand(fp, kLimitOffset));
  __ cmp(r0, r1);
  __ b(ne, &loop);

  // Invoke the function.
  ParameterCount actual(r0);
  __ mov(r0, Operand(r0, ASR, kSmiTagSize));
  __ ldr(r1, MemOperand(fp, kFunctionOffset));
  __ InvokeFunction(r1, actual, CALL_FUNCTION);

  // Tear down the internal frame and remove function, receiver and args.
  __ LeaveInternalFrame();
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ mov(pc, lr);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ mov(r4, Operand(ArgumentsAdaptorFrame::SENTINEL));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() | fp.bit() | lr.bit());
  __ add(fp, sp, Operand(3 * kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -3 * kPointerSize));
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  //  -- r3 : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ cmp(r0, Operand(r2));
  __ b(lt, &too_few);
  __ cmp(r2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address into r2.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r2, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: copy end address
    // r3: code entry to call

    Label copy;
    __ bind(&copy);
    __ ldr(ip, MemOperand(r0, 0));
    __ push(ip);
    __ cmp(r0, r2);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address is fp.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ ldr(ip, MemOperand(r0, 2 * kPointerSize));
    __ push(ip);
    __ cmp(r0, fp);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    // Fill the remaining expected arguments with undefined.
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ mov(ip, Operand(Factory::undefined_value()));
    __ sub(r2, fp, Operand(r2, LSL, kPointerSizeLog2));
    __ sub(r2, r2, Operand(4 * kPointerSize));  // Adjust for frame.

    Label fill;
    __ bind(&fill);
    __ push(ip);
    __ cmp(sp, r2);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ Call(r3);

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ mov(pc, lr);


  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ mov(pc, r3);
}


#undef __

} }  // namespace v8::internal
