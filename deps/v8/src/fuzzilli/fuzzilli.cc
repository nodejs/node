// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/fuzzilli/fuzzilli.h"

#include "include/v8-extension.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/api/api.h"
#include "src/codegen/external-reference.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/execution/simulator.h"
#include "src/fuzzilli/cov.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/testing.h"

#ifdef V8_OS_LINUX
#include <signal.h>
#include <unistd.h>
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

v8::Local<v8::FunctionTemplate> FuzzilliExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, FuzzilliExtension::Fuzzilli);
}

namespace {
void perform_wild_write() {
  // Access an invalid address.
  // We want to use an "interesting" address for the access (instead of
  // e.g. nullptr). In the (unlikely) case that the address is actually
  // mapped, simply increment the pointer until it crashes.
  // The cast ensures that this works correctly on both 32-bit and 64-bit.
  Address addr = static_cast<Address>(0x414141414141ull);
  char* ptr = reinterpret_cast<char*>(addr);
  for (int i = 0; i < 1024; i++) {
    *ptr = 'A';
    ptr += 1 * i::MB;
  }
}
}  // namespace

// We have to assume that the fuzzer will be able to call this function e.g. by
// enumerating the properties of the global object and eval'ing them. As such
// this function is implemented in a way that requires passing some magic value
// as first argument (with the idea being that the fuzzer won't be able to
// generate this value) which then also acts as a selector for the operation
// to perform.
void FuzzilliExtension::Fuzzilli(const FunctionCallbackInfo<Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  v8::String::Utf8Value operation(isolate, info[0]);
  if (*operation == nullptr) {
    return;
  }

  if (strcmp(*operation, "FUZZILLI_CRASH") == 0) {
    auto arg = info[1]
                   ->Int32Value(info.GetIsolate()->GetCurrentContext())
                   .FromMaybe(0);
    switch (arg) {
      case 0:
        IMMEDIATE_CRASH();
        break;
      case 1:
        CHECK(false);
        break;
      case 2:
        DCHECK(false);
        break;
      case 3: {
        perform_wild_write();
        break;
      }
      case 4: {
        // Use-after-free, should be caught by ASan (if active).
        auto* vec = new std::vector<int>(4);
        delete vec;
        USE(vec->at(0));
#ifndef V8_USE_ADDRESS_SANITIZER
        // The testcase must also crash on non-asan builds.
        perform_wild_write();
#endif  // !V8_USE_ADDRESS_SANITIZER
        break;
      }
      case 5: {
        // Out-of-bounds access (1), should be caught by libc++ hardening.
        std::vector<int> vec(5);
        USE(vec[5]);
        break;
      }
      case 6: {
        // Out-of-bounds access (2), should be caught by ASan (if active).
        std::vector<int> vec(6);
        memset(vec.data(), 42, 0x100);
#ifndef V8_USE_ADDRESS_SANITIZER
        // The testcase must also crash on non-asan builds.
        perform_wild_write();
#endif  // !V8_USE_ADDRESS_SANITIZER
        break;
      }
      case 7: {
        if (i::v8_flags.hole_fuzzing) {
          // This should crash with a segmentation fault only
          // when --hole-fuzzing is used.
          char* ptr = reinterpret_cast<char*>(0x414141414141ull);
          for (int i = 0; i < 1024; i++) {
            *ptr = 'A';
            ptr += 1 * i::GB;
          }
        }
        break;
      }
      case 8: {
        // This allows Fuzzilli to check that DEBUG is defined, which should be
        // the case if dcheck_always_on is set. This is useful for fuzzing as
        // there are some integrity checks behind DEBUG.
#ifdef DEBUG
        IMMEDIATE_CRASH();
#endif
        break;
      }
      case 9: {
        abort_with_sandbox_violation();
        break;
      }
      case 10: {  // SIGILL triggered by ud2
#ifdef V8_HOST_ARCH_X64
        __asm__ volatile("ud2\n");
#else
        fprintf(stderr, "Unsupported architecture for crash SIGILL crash\n");
#endif
        break;
      }
      case 11: {  // SIGILL triggered by non-ud2 invalid instruction
#ifdef V8_HOST_ARCH_X64
        // This instruction (0xFF 0xFF) is invalid on x64.
        __asm__ volatile(".byte 0xFF, 0xFF\n");
#else
        fprintf(stderr, "Unsupported architecture for crash SIGILL crash\n");
#endif
        break;
      }
      default:
        break;
    }
  } else if (strcmp(*operation, "FUZZILLI_PRINT") == 0) {
    static FILE* fzliout = fdopen(REPRL_DWFD, "w");
    if (!fzliout) {
      fprintf(
          stderr,
          "Fuzzer output channel not available, printing to stdout instead\n");
      fzliout = stdout;
    }

    v8::String::Utf8Value string(isolate, info[1]);
    if (*string == nullptr) {
      return;
    }
    fprintf(fzliout, "%s\n", *string);
    fflush(fzliout);
  }
}

}  // namespace internal
}  // namespace v8
