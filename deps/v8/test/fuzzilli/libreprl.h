// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LIBREPRL_H__
#define __LIBREPRL_H__

#include <sys/types.h>

struct reprl_child_process {
  // Read file descriptor of the control pipe.
  int crfd;
  // Write file descriptor of the control pipe.
  int cwfd;
  // Read file descriptor of the data pipe.
  int drfd;
  // Write file descriptor of the data pipe.
  int dwfd;
  // PID of the child process.
  int pid;
};

struct reprl_result {
  int child_died;
  int status;
  unsigned long exec_time;
  char* output;
  size_t output_size;
};

// Spawn a child process implementing the REPRL protocol.
int reprl_spawn_child(char** argv, char** envp,
                      struct reprl_child_process* child);

// Execute the provided script in the child process, wait for its completion,
// and return the result.
int reprl_execute_script(int pid, int crfd, int cwfd, int drfd, int dwfd,
                         int timeout, const char* script, int64_t script_length,
                         struct reprl_result* result);

#endif
