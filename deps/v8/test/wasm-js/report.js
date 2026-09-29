// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function check_test_results(tests, status) {
  const PASS = 0;
  const FAIL = 1;
  const TIMEOUT = 2;
  const NOTRUN = 3;
  const PRECONDITION_FAILED = 4;

  const status_names = {
    [PASS]: 'PASS',
    [FAIL]: 'FAIL',
    [TIMEOUT]: 'TIMEOUT',
    [NOTRUN]: 'NOT RUN',
    [PRECONDITION_FAILED]: 'PRECONDITION FAILED',
  };

  let failed_counter = 0;
  let passed_counter = 0;

  if (status.status != PASS) {
    console.log('Harness Error:');
    console.log(status.message);
    console.log(status.stack);
    failed_counter++;
  }

  for (let test of tests) {
    const status_label = test.status === PASS ? '[ PASS ]' : '[ FAIL ]';
    console.log(`${status_label} ${test.name}`);

    if (test.status !== PASS) {
      failed_counter++;
      const status_name =
          status_names[test.status] || `UNKNOWN (${test.status})`;
      console.log(`         Result: ${status_name}`);
      console.log(`         Message: ${test.message}`);
      if (test.stack) {
        console.log('         Stack trace:');
        console.log(test.stack.split('\n')
                        .map(line => `           ${line}`)
                        .join('\n'));
      }
    } else {
      passed_counter++;
    }
  }

  console.log(`\nSummary: ${tests.length} tests run, ${
      passed_counter} passed, ${failed_counter} failed.\n`);

  setTimeout(() => assertEquals(0, failed_counter));
}

add_completion_callback(check_test_results);
setup(() => {}, {'explicit_done': true});
