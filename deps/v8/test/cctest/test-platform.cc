// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "cctest.h"
#include "platform.h"

using namespace ::v8::internal;

#ifdef __GNUC__
#define ASM __asm__ __volatile__

#if defined(_M_X64) || defined(__x86_64__)
#define GET_STACK_POINTER() \
  static int sp_addr = 0; \
  do { \
    ASM("mov %%rsp, %0" : "=g" (sp_addr)); \
  } while (0)
#elif defined(_M_IX86) || defined(__i386__)
#define GET_STACK_POINTER() \
  static int sp_addr = 0; \
  do { \
    ASM("mov %%esp, %0" : "=g" (sp_addr)); \
  } while (0)
#elif defined(__ARMEL__)
#define GET_STACK_POINTER() \
  static int sp_addr = 0; \
  do { \
    ASM("str %%sp, %0" : "=g" (sp_addr)); \
  } while (0)
#elif defined(__MIPSEL__)
#define GET_STACK_POINTER() \
  static int sp_addr = 0; \
  do { \
    ASM("sw $sp, %0" : "=g" (sp_addr)); \
  } while (0)
#else
#error Host architecture was not detected as supported by v8
#endif

void GetStackPointer(const v8::FunctionCallbackInfo<v8::Value>& args) {
  GET_STACK_POINTER();
  args.GetReturnValue().Set(v8_num(sp_addr));
}


TEST(StackAlignment) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  global_template->Set(v8_str("get_stack_pointer"),
                       v8::FunctionTemplate::New(GetStackPointer));

  LocalContext env(NULL, global_template);
  CompileRun(
      "function foo() {"
      "  return get_stack_pointer();"
      "}");

  v8::Local<v8::Object> global_object = env->Global();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(global_object->Get(v8_str("foo")));

  v8::Local<v8::Value> result = foo->Call(global_object, 0, NULL);
  CHECK_EQ(0, result->Int32Value() % OS::ActivationFrameAlignment());
}

#undef GET_STACK_POINTERS
#undef ASM
#endif  // __GNUC__
