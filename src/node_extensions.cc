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


#include "node.h"
#include "node_version.h"
#include <string.h>
#include <stdio.h>

#undef NODE_EXT_LIST_START
#undef NODE_EXT_LIST_ITEM
#undef NODE_EXT_LIST_END

#define NODE_EXT_LIST_START
#define NODE_EXT_LIST_ITEM NODE_MODULE_DECL
#define NODE_EXT_LIST_END

#include "node_extensions.h"

#undef NODE_EXT_LIST_START
#undef NODE_EXT_LIST_ITEM
#undef NODE_EXT_LIST_END

#define NODE_EXT_STRING(x) &x ## _module,
#define NODE_EXT_LIST_START node::node_module_struct *node_module_list[] = {
#define NODE_EXT_LIST_ITEM NODE_EXT_STRING
#define NODE_EXT_LIST_END NULL};

#include "node_extensions.h"

namespace node {

node_module_struct* get_builtin_module(const char *name)
{
  char buf[128];
  node_module_struct *cur = NULL;
  snprintf(buf, sizeof(buf), "node_%s", name);
  /* TODO: you could look these up in a hash, but there are only
   * a few, and once loaded they are cached. */
  for (int i = 0; node_module_list[i] != NULL; i++) {
    cur = node_module_list[i];
    if (strcmp(cur->modname, buf) == 0) {
      return cur;
    }
  }

  return NULL;
}

}; // namespace node
