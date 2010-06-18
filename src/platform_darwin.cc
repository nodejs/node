#include "node.h"
#include "platform.h"

#include <mach/task.h>
#include <mach/mach_init.h>

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


}  // namespace node
