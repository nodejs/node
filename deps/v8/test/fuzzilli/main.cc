// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libreprl.h"

struct reprl_context* ctx;

int execute(const char* code) {
  uint64_t exec_time;
  return reprl_execute(ctx, code, strlen(code), 1000, &exec_time, 0);
}

void expect_success(const char* code) {
  if (execute(code) != 0) {
    printf("Execution of \"%s\" failed\n", code);
    exit(1);
  }
}

void expect_failure(const char* code) {
  if (execute(code) == 0) {
    printf("Execution of \"%s\" unexpectedly succeeded\n", code);
    exit(1);
  }
}

int main(int argc, char** argv) {
  ctx = reprl_create_context();

  const char* env[] = {nullptr};
  const char* prog = argc > 1 ? argv[1] : "./out.gn/x64.debug/d8";
  const char* args[] = {prog, nullptr};
  if (reprl_initialize_context(ctx, args, env, 1, 1) != 0) {
    printf("REPRL initialization failed\n");
    return -1;
  }

  // Basic functionality test
  if (execute("let greeting = \"Hello World!\";") != 0) {
    printf(
        "Script execution failed, is %s the path to d8 built with "
        "v8_fuzzilli=true?\n",
        prog);
    return -1;
  }

  // Verify that runtime exceptions can be detected
  expect_failure("throw 'failure';");

  // Verify that existing state is property reset between executions
  expect_success("globalProp = 42; Object.prototype.foo = \"bar\";");
  expect_success("if (typeof(globalProp) !== 'undefined') throw 'failure'");
  expect_success("if (typeof(({}).foo) !== 'undefined') throw 'failure'");

  // Verify that rejected promises are properly reset between executions
  expect_failure("async function fail() { throw 42; }; fail()");
  expect_success("42");
  expect_failure("async function fail() { throw 42; }; fail()");

  puts("OK");
  return 0;
}
}
