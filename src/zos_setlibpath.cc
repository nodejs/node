// This is linked to the node executable, so the global libpath object's
// constructor searches for libnode DLL and sets LIBPATH, so libnode.so can be
// loaded.

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ps.h>
#include <unistd.h>

#include <string>

namespace {
/* Append dir to the colon-separated list unless it's already in list. */
void append_unique(std::string& list, const std::string& dir) {
  if ((":" + list + ":").find(":" + dir + ":") != std::string::npos) return;
  if (!list.empty()) list += ':';
  list += dir;
}

class __setlibpath {
 public:
  __setlibpath() {
    char argv[PATH_MAX];
    char parent[PATH_MAX], *p;
    W_PSPROC buf;
    int token = 0;
    pid_t mypid = getpid();
    memset(&buf, 0, sizeof(buf));
    buf.ps_pathlen = sizeof(argv);
    buf.ps_pathptr = argv;
    while ((token = w_getpsent(token, &buf, sizeof(buf))) > 0) {
      if (buf.ps_pid == mypid) {
        /* Found our process. */

        /*
         * Resolve path to find true location of executable; don't use an
         * overridden realpath function from zoslib, since this header may
         * be used in an exe before libzoslib.so has been loaded.
         */
        if (__realpath_a(argv, parent) == nullptr) {
          fprintf(stderr,
                  "Error: __realpath_a(%s) failed, errno=%d.\n"
                  "Please report this error to IBM customer support.\n",
                  argv,
                  errno);
          return;
        }

        /* Get parent directory. */
        p = dirname(parent);
        if (p == nullptr) {
          fprintf(stderr,
                  "Error: dirname(%s) failed, errno=%d (argv=%s).\n"
                  "Please report this error to IBM customer support.\n",
                  parent,
                  errno,
                  argv);
          return;
        }

        /* Get parent's parent directory. */
        char parent2[PATH_MAX + 1], *p2;
        strncpy(parent2, parent, sizeof(parent2) - 1);
        parent2[sizeof(parent2) - 1] = 0;
        p2 = dirname(parent2);
        if (p2 == nullptr) {
          fprintf(stderr,
                  "Error: dirname(%s) for parent failed, "
                  "errno=%d (argv=%s).\n"
                  "Please report this error to IBM customer support.\n",
                  parent2,
                  errno,
                  argv);
          return;
        }

        /* Append new paths to libpath, skipping any already present. */
        const char* env = getenv("LIBPATH");
        std::string libpath = env != nullptr ? env : "";
        append_unique(libpath, std::string(p2) + "/lib");
        append_unique(libpath, std::string(p) + "/lib");
        append_unique(libpath, std::string(p) + "/lib.target");
        setenv("LIBPATH", libpath.c_str(), 1);
        return;
      }
    }
    fprintf(stderr,
            "Error: w_getpsent failed to find process id %d, "
            "errno=%d.\n"
            "Please report this error to IBM customer support.\n",
            mypid,
            errno);
  }
};

__setlibpath libpath;
}  // namespace
