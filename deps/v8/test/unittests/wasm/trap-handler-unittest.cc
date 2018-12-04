// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/trap-handler/trap-handler.h"
#include "include/v8.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_OS_POSIX
#include <setjmp.h>
#include <signal.h>
#endif

namespace {

#if V8_TRAP_HANDLER_SUPPORTED

void CrashOnPurpose() { *reinterpret_cast<volatile int*>(42); }

// When using V8::RegisterDefaultSignalHandler, we save the old one to fall back
// on if V8 doesn't handle the signal. This allows tools like ASan to register a
// handler early on during the process startup and still generate stack traces
// on failures.
class SignalHandlerFallbackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    struct sigaction action;
    action.sa_sigaction = SignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &action, &old_segv_action_);
    sigaction(SIGBUS, &action, &old_bus_action_);
  }

  void TearDown() override {
    // be a good citizen and restore the old signal handler.
    sigaction(SIGSEGV, &old_segv_action_, nullptr);
    sigaction(SIGBUS, &old_bus_action_, nullptr);
  }

  static sigjmp_buf continuation_;

 private:
  static void SignalHandler(int signal, siginfo_t* info, void*) {
    siglongjmp(continuation_, 1);
  }
  struct sigaction old_segv_action_;
  struct sigaction old_bus_action_;  // We get SIGBUS on Mac sometimes.
};
sigjmp_buf SignalHandlerFallbackTest::continuation_;

TEST_F(SignalHandlerFallbackTest, DoTest) {
  const int save_sigs = 1;
  if (!sigsetjmp(continuation_, save_sigs)) {
    constexpr bool use_default_signal_handler = true;
    CHECK(v8::V8::EnableWebAssemblyTrapHandler(use_default_signal_handler));
    CrashOnPurpose();
    FAIL();
  } else {
    // Our signal handler ran.
    v8::internal::trap_handler::RestoreOriginalSignalHandler();
    SUCCEED();
    return;
  }
  FAIL();
}

#endif

}  //  namespace
