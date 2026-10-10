// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_zos.h"
#include "env-inl.h"
#include "node_internals.h"
#include "node_watchdog.h"

#include <pthread.h>
#include <signal.h>
#include <sys/msg.h>

#ifndef NJS_PRODUCT_OWNER
#error "NJS_PRODUCT_OWNER must be defined"
#endif

#ifndef NJS_FEATURE_NAME
#error "NJS_FEATURE_NAME must be defined"
#endif

#ifndef NJS_PRODUCT_NAME
#error "NJS_PRODUCT_NAME must be defined"
#endif

#ifndef NJS_PID
#error "NJS_PID must be defined"
#endif

namespace {

uv_thread_t signalHandlerThread;
char* galtStack = nullptr;

void __zosAtExit() {
  if (galtStack != nullptr) {
    free(galtStack);
    galtStack = nullptr;
  }
}
}  // namespace

namespace node {

void zosDestroyThread() {
  uv_queue_work(nullptr, nullptr, nullptr, nullptr);
  SigintWatchdogHelper::GetInstance()->Stop();
  msgctl(uv_backend_fd(uv_default_loop()), IPC_RMID, nullptr);
  // Tell zoslib that main process is terminating, for its diagnostics:
  __mainTerminating();
}

static void SignalHandlerThread(void* data) {
  while (1) {
    CHECK_EQ(pause(), -1);
    CHECK_EQ(errno, EINTR);
  }
}

void zosCreateSignalHandler() {
  uv_thread_create(&signalHandlerThread, SignalHandlerThread, nullptr);
  // Block all signals on the main thread
  sigset_t set;
  sigfillset(&set);
  CHECK_EQ(0, pthread_sigmask(SIG_BLOCK, &set, nullptr));
}

void zosCancelSignalHandler() {
  if (pthread_cancel(signalHandlerThread) == -1) abort();
}

void zosSignalExit(int signo, siginfo_t* info, void* ucontext) {
  zosDestroyThread();
  // Use alternate stack in case LE uses main thread's sp upon termination.
  struct sigaction new_action;
  new_action.sa_flags = SA_ONSTACK;
  new_action.sa_handler = SIG_DFL;
  sigaction(signo, &new_action, nullptr);
  if (signo == SIGABRT)
    abort();  // from zoslib
  else
    raise(signo);
}

void zosIgnorePipeChildSignals() {
  struct sigaction new_action;
  new_action.sa_handler = SIG_IGN;
  sigfillset(&new_action.sa_mask);
  sigaction(SIGPIPE, &new_action, nullptr);
  new_action.sa_handler = SIG_IGN;
  sigfillset(&new_action.sa_mask);
  sigaction(SIGCHLD, &new_action, nullptr);
}

static void RegisterProduct() {
  std::string major_version = std::to_string(NODE_MAJOR_VERSION);
  std::string product_owner = NJS_PRODUCT_OWNER;
  std::string feature_name = NJS_FEATURE_NAME;
  std::string product_name = NJS_PRODUCT_NAME;
  std::string pid = NJS_PID;

  std::string val;
  if (credentials::SafeGetenv("NODE_SMF89_REGISTRATION_VERBOSE", &val))
    printf("Product registration data:\n"
           "Product Name: %s\n"
           "PID Name: %s\n"
           "Major Version: %s\n"
           "Product Owner: %s\n"
           "Feature Name: %s\n",
           product_name.c_str(),
           pid.c_str(),
           major_version.c_str(),
           product_owner.c_str(),
           feature_name.c_str());

  uint64_t rc = __registerProduct(major_version.c_str(),
                                  product_owner.c_str(),
                                  feature_name.c_str(),
                                  product_name.c_str(),
                                  pid.c_str());

  if (rc && !credentials::SafeGetenv("NODE_SMF89_SUPPRESS_WARNING", &val)) {
    fprintf(stderr,
            "WARNING: Could not register product with IFAUSAGE, "
            "rc = %lu\n",
            rc);
    if (const char* errorString = getIFAUsageErrorString(rc))
      fprintf(stderr, "%s\n", errorString);

    fprintf(stderr,
            "WARNING: Product usage data may not be collected. For more "
            "details on the product registration data, set environment "
            "variable NODE_SMF89_REGISTRATION_VERBOSE=1. To suppress this "
            "warning message set the environment variable "
            "NODE_SMF89_SUPPRESS_WARNING=1.\n");
  }
}

ExitCode zosStart() {
  // Initialize environment variables in zoslib:
  zoslib_config_t config{
      .RUNTIME_LIMIT_ENVAR = "__NODERUNTIMELIMIT",
      .CCSID_GUESS_BUF_SIZE_ENVAR = "__NODECCSIDGUESSBUFSIZE"};
  if (__update_envar_names(&config) != 0) {
    return ExitCode::kGenericUserError;
  }

  // Create an alternate stack for signal processing
  const unsigned int STACK_SIZE = SIGSTKSZ + 1024 * 1024;
  galtStack = reinterpret_cast<char*>(malloc(STACK_SIZE));
  if (galtStack == nullptr) {
    return ExitCode::kGenericUserError;
  }
  stack_t ss = {.ss_sp = galtStack, .ss_size = STACK_SIZE, .ss_flags = 0};
  sigaltstack(&ss, 0);

  if (__doLogMemoryUsage()) atexit(__zosAtExit);

  return ExitCode::kNoFailure;
}

}  // namespace node
