#include "node.h"
#include "platform.h"

#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <limits.h> /* PATH_MAX */

namespace node {

// Researched by Tim Becker and Michael Knight
// http://blog.kuriositaet.de/?p=257
int OS::GetMemory(size_t *rss, size_t *vsize) {
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  int r = task_info(mach_task_self(),
                    TASK_BASIC_INFO,
                    (task_info_t)&t_info,
                    &t_info_count);

  if (r != KERN_SUCCESS) return -1;

  *rss = t_info.resident_size;
  *vsize  = t_info.virtual_size;

  return 0;
}


int OS::GetExecutablePath(char* buffer, size_t* size) {
  uint32_t usize = *size;
  int result = _NSGetExecutablePath(buffer, &usize);
  if (result) return result;

  char *path = new char[2*PATH_MAX];

  char *fullpath = realpath(buffer, path);
  if (fullpath == NULL) {
    delete [] path;
    return -1;
  }
  strncpy(buffer, fullpath, *size);
  delete [] fullpath;
  *size = strlen(buffer);
  return 0;
}

}  // namespace node
