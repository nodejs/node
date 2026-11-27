// z/OS-specific process setup: alternate signal stack, signal handler
// threads and product registration. Used by node.cc and inspector_agent.cc;
// compiled on z/OS only.

#include "node_zos.h"
#include "env-inl.h"
#include "node_internals.h"
#include "node_watchdog.h"

#include <pthread.h>
#include <signal.h>
#include <sys/msg.h>

#include <string>

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
// Size of the alternate signal stack of the main thread and of the threads
// that receive signals (see zosSetThreadAltStack()).
const unsigned int kAltStackSize = SIGSTKSZ + 1024 * 1024;

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

void zosSetThreadAltStack() {
  // sigaltstack() is per-thread, so a thread that may receive signals needs
  // its own alternate stack for SA_ONSTACK handlers to run on it.
  // The stack is intentionally never freed: the callers live until process
  // exit.
  char* alt_stack = static_cast<char*>(malloc(kAltStackSize));
  CHECK_NOT_NULL(alt_stack);
  stack_t ss = {.ss_sp = alt_stack, .ss_size = kAltStackSize, .ss_flags = 0};
  CHECK_EQ(0, sigaltstack(&ss, nullptr));
}

static void SignalHandlerThread(void* data) {
  // Process-directed signals are delivered to this thread (all signals are
  // blocked on the others), so it needs its own alternate stack.
  zosSetThreadAltStack();

  // This thread was created with all signals blocked, so that none could be
  // delivered before the alternate stack was installed. Unblock them now.
  sigset_t set;
  sigfillset(&set);
#if HAVE_INSPECTOR
  // Keep SIGUSR1 blocked, as PlatformInit() left it, until the inspector
  // installs its handler (see SigUsr1Posting() in inspector_agent.cc).
  sigdelset(&set, SIGUSR1);
#endif
  CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &set, nullptr));

  while (true) {
    CHECK_EQ(pause(), -1);
    CHECK_EQ(errno, EINTR);
  }
}

void zosCreateSignalHandler() {
  // Block all signals on the main thread. The signal handler thread inherits
  // this mask and unblocks the signals once its alternate stack is set.
  sigset_t set;
  sigfillset(&set);
  CHECK_EQ(0, pthread_sigmask(SIG_BLOCK, &set, nullptr));
  CHECK_EQ(
      0, uv_thread_create(&signalHandlerThread, SignalHandlerThread, nullptr));
}

void zosCancelSignalHandler() {
  if (pthread_cancel(signalHandlerThread) != 0) abort();
}

void zosSignalExit(int signo, siginfo_t* info, void* ucontext) {
  zosDestroyThread();
  // Use alternate stack in case LE uses main thread's sp upon termination.
  struct sigaction new_action = {};
  new_action.sa_flags = SA_ONSTACK;
  new_action.sa_handler = SIG_DFL;
  sigaction(signo, &new_action, nullptr);
  if (signo == SIGABRT)
    abort();  // from zoslib
  else
    raise(signo);
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
  galtStack = reinterpret_cast<char*>(malloc(kAltStackSize));
  if (galtStack == nullptr) {
    fprintf(stderr, "Error: could not allocate the alternate signal stack.\n");
    return ExitCode::kGenericUserError;
  }

  stack_t ss = {.ss_sp = galtStack, .ss_size = kAltStackSize, .ss_flags = 0};
  if (sigaltstack(&ss, 0) != 0) {
    fprintf(stderr, "Error: sigaltstack() failed, errno=%d.\n", errno);
    return ExitCode::kGenericUserError;
  }

  if (__doLogMemoryUsage()) atexit(__zosAtExit);

  // Register Node.js for Tailored Fit Pricing.
  RegisterProduct();

  return ExitCode::kNoFailure;
}

}  // namespace node
