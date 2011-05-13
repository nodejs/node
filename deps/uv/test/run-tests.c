/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "runner.h"
#include "task.h"

/* Actual tests and helpers are defined in test-list.h */
#include "test-list.h"


/* The time in milliseconds after which a single test times out. */
#define TEST_TIMEOUT  5000


static void log_progress(int total, int passed, int failed, char* name) {
  LOGF("[%% %3d|+ %3d|- %3d]: %s", (passed + failed) / total * 100,
      passed, failed, name);
}


int main(int argc, char **argv) {
  int total, passed, failed;
  task_entry_t* task;

  platform_init(argc, argv);

  if (argc > 1) {
    /* A specific process was requested. */
    return run_process(argv[1]);

  } else {
    /* Count the number of tests. */
    total = 0;
    task = (task_entry_t*)&TASKS;
    for (task = (task_entry_t*)&TASKS; task->main; task++) {
      if (!task->is_helper) {
        total++;
      }
    }

    /* Run all tests. */
    passed = 0;
    failed = 0;
    task = (task_entry_t*)&TASKS;
    for (task = (task_entry_t*)&TASKS; task->main; task++) {
      if (task->is_helper) {
        continue;
      }

      rewind_cursor();
      log_progress(total, passed, failed, task->task_name);

      if (run_task(task, TEST_TIMEOUT, 0)) {
        passed++;
      } else {
        failed++;
      }
    }

    rewind_cursor();
    log_progress(total, passed, failed, "Done.\n");

    return 0;
  }
}
