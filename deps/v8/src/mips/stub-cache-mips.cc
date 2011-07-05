// Copyright 2010 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_MIPS)

#include "ic-inl.h"
#include "codegen-inl.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra,
                              Register extra2) {
  UNIMPLEMENTED_MIPS();
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  UNIMPLEMENTED_MIPS();
}


// Load a fast property out of a holder object (src). In-object properties
// are loaded directly otherwise the property is loaded from the properties
// fixed array.
void StubCompiler::GenerateFastPropertyLoad(MacroAssembler* masm,
                                            Register dst, Register src,
                                            JSObject* holder, int index) {
  UNIMPLEMENTED_MIPS();
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  UNIMPLEMENTED_MIPS();
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  UNIMPLEMENTED_MIPS();
}


// Generate StoreField code, value is passed in r0 register.
// After executing generated code, the receiver_reg and name_reg
// may be clobbered.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      JSObject* object,
                                      int index,
                                      Map* transition,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch,
                                      Label* miss_label) {
  UNIMPLEMENTED_MIPS();
}


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  UNIMPLEMENTED_MIPS();
}


#undef __
#define __ ACCESS_MASM(masm())


void StubCompiler::GenerateLoadField(JSObject* object,
                                     JSObject* holder,
                                     Register receiver,
                                     Register scratch1,
                                     Register scratch2,
                                     int index,
                                     String* name,
                                     Label* miss) {
  UNIMPLEMENTED_MIPS();
}


void StubCompiler::GenerateLoadConstant(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Object* value,
                                        String* name,
                                        Label* miss) {
  UNIMPLEMENTED_MIPS();
}


bool StubCompiler::GenerateLoadCallback(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        AccessorInfo* callback,
                                        String* name,
                                        Label* miss,
                                        Failure** failure) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x470);
  return false;   // UNIMPLEMENTED RETURN
}


void StubCompiler::GenerateLoadInterceptor(JSObject* object,
                                           JSObject* holder,
                                           LookupResult* lookup,
                                           Register receiver,
                                           Register name_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           String* name,
                                           Label* miss) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x505);
}


Object* StubCompiler::CompileLazyCompile(Code::Flags flags) {
  // Registers:
  // a1: function
  // ra: return address

  // Enter an internal frame.
  __ EnterInternalFrame();
  // Preserve the function.
  __ Push(a1);
  // Setup aligned call.
  __ SetupAlignedCall(t0, 1);
  // Push the function on the stack as the argument to the runtime function.
  __ Push(a1);
  // Call the runtime function
  __ CallRuntime(Runtime::kLazyCompile, 1);
  __ ReturnFromAlignedCall();
  // Calculate the entry point.
  __ addiu(t9, v0, Code::kHeaderSize - kHeapObjectTag);
  // Restore saved function.
  __ Pop(a1);
  // Tear down temporary frame.
  __ LeaveInternalFrame();
  // Do a tail-call of the compiled function.
  __ Jump(t9);

  return GetCodeWithFlags(flags, "LazyCompileStub");
}


Object* CallStubCompiler::CompileCallField(JSObject* object,
                                           JSObject* holder,
                                           int index,
                                           String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* CallStubCompiler::CompileArrayPushCall(Object* object,
                                               JSObject* holder,
                                               JSFunction* function,
                                               String* name,
                                               CheckType check) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* CallStubCompiler::CompileArrayPopCall(Object* object,
                                              JSObject* holder,
                                              JSFunction* function,
                                              String* name,
                                              CheckType check) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* CallStubCompiler::CompileCallConstant(Object* object,
                                              JSObject* holder,
                                              JSFunction* function,
                                              String* name,
                                              CheckType check) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* CallStubCompiler::CompileCallInterceptor(JSObject* object,
                                                 JSObject* holder,
                                                 String* name) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x782);
  return GetCode(INTERCEPTOR, name);
}


Object* CallStubCompiler::CompileCallGlobal(JSObject* object,
                                            GlobalObject* holder,
                                            JSGlobalPropertyCell* cell,
                                            JSFunction* function,
                                            String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* StoreStubCompiler::CompileStoreField(JSObject* object,
                                             int index,
                                             Map* transition,
                                             String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* StoreStubCompiler::CompileStoreCallback(JSObject* object,
                                                AccessorInfo* callback,
                                                String* name) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x906);
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                   String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
                                              JSGlobalPropertyCell* cell,
                                              String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* LoadStubCompiler::CompileLoadField(JSObject* object,
                                           JSObject* holder,
                                           int index,
                                           String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* LoadStubCompiler::CompileLoadCallback(String* name,
                                              JSObject* object,
                                              JSObject* holder,
                                              AccessorInfo* callback) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* LoadStubCompiler::CompileLoadConstant(JSObject* object,
                                              JSObject* holder,
                                              Object* value,
                                              String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* LoadStubCompiler::CompileLoadInterceptor(JSObject* object,
                                                 JSObject* holder,
                                                 String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* LoadStubCompiler::CompileLoadGlobal(JSObject* object,
                                            GlobalObject* holder,
                                            JSGlobalPropertyCell* cell,
                                            String* name,
                                            bool is_dont_delete) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int index) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   Object* value) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


// TODO(1224671): implement the fast case.
Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                  int index,
                                                  Map* transition,
                                                  String* name) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* ConstructStubCompiler::CompileConstructStub(
    SharedFunctionInfo* shared) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* ExternalArrayStubCompiler::CompileKeyedLoadStub(
    ExternalArrayType array_type, Code::Flags flags) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


Object* ExternalArrayStubCompiler::CompileKeyedStoreStub(
    ExternalArrayType array_type, Code::Flags flags) {
  UNIMPLEMENTED_MIPS();
  return reinterpret_cast<Object*>(NULL);   // UNIMPLEMENTED RETURN
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
