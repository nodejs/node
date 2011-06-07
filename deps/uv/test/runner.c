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

#include <string.h>

#include "runner.h"
#include "task.h"

char executable_path[PATHMAX] = { '\0' };

/* Start a specific process declared by TEST_ENTRY or TEST_HELPER. */
/* Returns the exit code of the specific process. */
int run_process(char* name) {
  task_entry_t *test;

  for (test = (task_entry_t*)&TASKS; test->main; test++) {
    if (strcmp(name, test->process_name) == 0) {
      return test->main();
    }
  }

  LOGF("Test process %s not found!\n", name);
  return 255;
}


/*
 * Runs all processes associated with a particular test or benchmark.
 * It returns 1 if the test succeeded, 0 if it failed.
 * If the test fails it prints diagnostic information.
 * If benchmark_output is nonzero, the output from the main process is
 * always shown.
 */
int run_task(task_entry_t *test, int timeout, int benchmark_output) {
  int i, result, success;
  char errmsg[256];
  task_entry_t *helper;
  int process_count;
  process_info_t processes[MAX_PROCESSES];
  process_info_t *main_process;

  success = 0;

  process_count = 0;

  /* Start all helpers for this test first. */
  for (helper = (task_entry_t*)&TASKS; helper->main; helper++) {
    if (helper->is_helper &&
        strcmp(test->task_name, helper->task_name) == 0) {
      if (process_start(helper->process_name, &processes[process_count]) == -1) {
        snprintf((char*)&errmsg,
                 sizeof(errmsg),
                 "process `%s` failed to start.",
                 helper->process_name);
        goto finalize;
      }
      process_count++;
    }
  }

  /* Wait a little bit to allow servers to start. Racy. */
  uv_sleep(50);

  /* Start the main test process. */
  if (process_start(test->process_name, &processes[process_count]) == -1) {
    snprintf((char*)&errmsg, sizeof(errmsg), "process `%s` failed to start.",
        test->process_name);
    goto finalize;
  }
  main_process = &processes[process_count];
  process_count++;

  /* Wait for the main process to terminate. */
  result = process_wait(main_process, 1, timeout);
  if (result == -1) {
    FATAL("process_wait failed");
  } else if (result == -2) {
    snprintf((char*)&errmsg, sizeof(errmsg), "timeout.");
    goto finalize;
  }

  /* Reap the main process. */
  result = process_reap(main_process);
  if (result != 0) {
    snprintf((char*)&errmsg, sizeof(errmsg), "exit code %d.", result);
    goto finalize;
  }

  /* Yes! did it. */
  success = 1;

finalize:
  /* Kill all (helper) processes that are still running. */
  for (i = 0; i < process_count; i++) {
    /* If terminate fails the process is probably already closed. */
    process_terminate(&processes[i]);
  }

  /* Wait until all processes have really terminated. */
  if (process_wait((process_info_t*)&processes, process_count, -1) < 0) {
    FATAL("process_wait failed");
  }

  /* Show error and output from processes if the test failed. */
  if (!success) {
    LOGF("\n`%s` failed: %s\n", test->task_name, errmsg);

    for (i = 0; i < process_count; i++) {
      switch (process_output_size(&processes[i])) {
       case -1:
        LOGF("Output from process `%s`: (unavailable)\n",
             process_get_name(&processes[i]));
        break;

       case 0:
        LOGF("Output from process `%s`: (no output)\n",
             process_get_name(&processes[i]));
        break;

       default:
        LOGF("Output from process `%s`:\n", process_get_name(&processes[i]));
        process_copy_output(&processes[i], fileno(stderr));
        break;
      }
    }
    LOG("=============================================================\n");

  /* In benchmark mode show concise output from the main process. */
  } else if (benchmark_output) {
    switch (process_output_size(main_process)) {
     case -1:
      LOGF("%s: (unavailabe)\n", test->task_name);
      break;

     case 0:
      LOGF("%s: (no output)\n", test->task_name);
      break;

     default:
      //LOGF("%s: ", test->task_name);
      process_copy_output(main_process, fileno(stderr));
      break;
    }
  }

  /* Clean up all process handles. */
  for (i = 0; i < process_count; i++) {
    process_cleanup(&processes[i]);
  }

  return success;
}
