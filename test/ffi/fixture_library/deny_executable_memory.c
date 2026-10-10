#include <errno.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

// The ignored function pointer keeps this fixture off the Fast API path, so it
// can install the restriction before Node's first executable-memory probe.
int deny_executable_memory(void (*unused)(void)) {
  (void)unused;
#if defined(__NR_mmap) && defined(__NR_mprotect)
  struct sock_filter filter[] = {
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mprotect, 2, 0),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mmap, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               offsetof(struct seccomp_data, args[2])),
      BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, PROT_EXEC, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EACCES),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
  };
  struct sock_fprog program = {
      .len = sizeof(filter) / sizeof(filter[0]),
      .filter = filter,
  };

  // SAFETY: Only the test thread is restricted, and the filter returns EACCES
  // for executable mappings rather than terminating it. The test uses jitless.
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 ||
      prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) != 0) {
    return errno;
  }
  return 0;
#else
  return ENOSYS;
#endif
}
