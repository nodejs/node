// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include "libreprl.h"
}
int main() {
  struct reprl_child_process child;
  char* env[] = {nullptr};
  char prog[] = "./out.gn/x64.debug/d8";
  char*(argv[]) = {prog, nullptr};
  if (reprl_spawn_child(argv, env, &child) == -1) return -1;
  // struct reprl_result res;
  // reprl_execute_script(child.pid, child.crfd, child.cwfd, child.drfd,
  // child.dwfd, 1, ,,&res);
  return 0;
}
