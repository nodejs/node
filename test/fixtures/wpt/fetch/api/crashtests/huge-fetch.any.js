// META: global=window,worker

'use strict';

promise_test(async t => {
  const response = await fetch('../resources/huge-response.py');
  const reader = response.body.getReader();
  // Read one chunk just to show willing.
  const { value, done } = await reader.read();
  assert_false(done, 'there should be some data');
  assert_greater_than(value.byteLength, 0, 'the chunk should be non-empty');
  // Wait 2 seconds to give it a chance to crash.
  await new Promise(resolve => t.step_timeout(resolve, 2000));
  // If we get here without crashing we passed the test.
  reader.cancel();
}, 'fetching a huge cacheable file but not reading it should not crash');
