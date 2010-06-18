#include "node.h"
#include "platform.h"

# include <sys/param.h> /* for MAXPATHLEN */


namespace node {


int OS::GetMemory(size_t *rss, size_t *vsize) {
  FILE *f = fopen("/proc/self/stat", "r");
  if (!f) return -1;

  int itmp;
  char ctmp;
  char buffer[MAXPATHLEN];
  size_t page_size = getpagesize();

  /* PID */
  if (fscanf(f, "%d ", &itmp) == 0) goto error;
  /* Exec file */
  if (fscanf (f, "%s ", &buffer[0]) == 0) goto error;
  /* State */
  if (fscanf (f, "%c ", &ctmp) == 0) goto error;
  /* Parent process */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* Process group */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* Session id */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* TTY */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* TTY owner process group */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* Flags */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Minor faults (no memory page) */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Minor faults, children */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Major faults (memory page faults) */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Major faults, children */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* utime */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* stime */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* utime, children */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* stime, children */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* jiffies remaining in current time slice */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* 'nice' value */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;
  /* jiffies until next timeout */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* jiffies until next SIGALRM */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* start time (jiffies since system boot) */
  if (fscanf (f, "%d ", &itmp) == 0) goto error;

  /* Virtual memory size */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  *vsize = (size_t) itmp;

  /* Resident set size */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  *rss = (size_t) itmp * page_size;

  /* rlim */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Start of text */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* End of text */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;
  /* Start of stack */
  if (fscanf (f, "%u ", &itmp) == 0) goto error;

  fclose (f);

  return 0;

error:
  fclose (f);
  return -1;
}


}  // namespace node
