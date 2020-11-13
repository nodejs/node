// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {

#include <stdio.h>
#include <string.h>

#include "libreprl.h"

int main(int argc, char** argv) {
  struct reprl_context* ctx = reprl_create_context();

  const char* env[] = {nullptr};
  const char* prog = argc > 1 ? argv[1] : "./out.gn/x64.debug/d8";
  const char* args[] = {prog, nullptr};
  if (reprl_initialize_context(ctx, args, env, 1, 1) != 0) {
    printf("REPRL initialization failed\n");
    return -1;
  }

  uint64_t exec_time;

  // Basic functionality test
  const char* code = "let greeting = \"Hello World!\";";
  if (reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0) != 0) {
    printf("Execution of \"%s\" failed\n", code);
    printf("Is %s the path to d8 built with v8_fuzzilli=true?\n", prog);
    return -1;
  }

  // Verify that runtime exceptions can be detected
  code = "throw 'failure';";
  if (reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0) == 0) {
    printf("Execution of \"%s\" unexpectedly succeeded\n", code);
    return -1;
  }

  // Verify that existing state is property reset between executions
  code = "globalProp = 42; Object.prototype.foo = \"bar\";";
  if (reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0) != 0) {
    printf("Execution of \"%s\" failed\n", code);
    return -1;
  }
  code = "if (typeof(globalProp) !== 'undefined') throw 'failure'";
  if (reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0) != 0) {
    printf("Execution of \"%s\" failed\n", code);
    return -1;
  }
  code = "if (typeof(Object.prototype.foo) !== 'undefined') throw 'failure'";
  if (reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0) != 0) {
    printf("Execution of \"%s\" failed\n", code);
    return -1;
  }

  puts("OK");
  return 0;
}
}
