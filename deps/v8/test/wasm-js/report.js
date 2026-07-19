// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function check_test_results(tests, status) {
  const PASS = 0;
  const FAIL = 1;
  const TIMEOUT = 2;
  const NOTRUN = 3;
  const PRECONDITION_FAILED = 4;

  let failed_counter = 0;
  if (status.status != PASS) {
    console.log("Error:");
    console.log(status.message);
    console.log(status.stack);
    failed_counter++;
  }
  for (let test of tests) {
    if (test.status !== PASS) {
      console.log(`\nTest: ${test.name}`);
      if (test.status === FAIL) {
        console.log("Test failed");
      } else if (test.status === TIMEOUT) {
        console.log("Timeout");
      } else if (test.status === NOTRUN) {
        console.log("Test did not run");
      } else if (test.status === PRECONDITION_FAILED) {
        console.log("Test precondition failed");
      } else {
        console.log("Unknown error code:", test.status);
      }
      console.log(`Message: ${test.message}`);
      console.log("Stack trace:");
      console.log(test.stack);
      failed_counter++;
    }
  }

  setTimeout(() => assertEquals(0, failed_counter));
}

add_completion_callback(check_test_results);
setup(() => {}, {'explicit_done': true});
