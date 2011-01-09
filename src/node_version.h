
#include "node_config.h"

#ifndef NODE_VERSION_H
#define NODE_VERSION_H

#define NODE_MAJOR_VERSION 0
#define NODE_MINOR_VERSION 3
#define NODE_PATCH_VERSION 5
#define NODE_VERSION_IS_RELEASE 0

#ifndef NODE_STRINGIFY
#define NODE_STRINGIFY(n) NODE_STRINGIFY_HELPER(n)
#define NODE_STRINGIFY_HELPER(n) #n
#endif

#if NODE_VERSION_IS_RELEASE
# define NODE_VERSION_STRING  NODE_STRINGIFY(NODE_MAJOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_MINOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_PATCH_VERSION)
#else
# define NODE_VERSION_STRING  NODE_STRINGIFY(NODE_MAJOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_MINOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_PATCH_VERSION) "-pre"
#endif

#define NODE_VERSION "v" NODE_VERSION_STRING


#define NODE_VERSION_AT_LEAST(major, minor, patch) \
  (( (major) < NODE_MAJOR_VERSION) \
  || ((major) == NODE_MAJOR_VERSION && (minor) < NODE_MINOR_VERSION) \
  || ((major) == NODE_MAJOR_VERSION && (minor) == NODE_MINOR_VERSION && (patch) <= NODE_PATCH_VERSION))

#endif /* NODE_VERSION_H */
