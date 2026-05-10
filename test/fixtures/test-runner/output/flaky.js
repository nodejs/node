// Flags: --test-reporter=tap
'use strict';
require('../../../common');
const { test, describe, it } = require('node:test');

// Track invocation count for flaky tests
let flakyTestInvocations = 0;
let flakyTestMethodInvocations = 0;
let flakyTestAlwaysFails = 0;

// Test that passes after 2 retries (3rd attempt succeeds)
test('flaky test that passes on retry', { flaky: 5 }, () => {
  flakyTestInvocations++;
  if (flakyTestInvocations < 3) {
    throw new Error('Simulated failure');
  }
  // Passes on 3rd attempt
});

// Test using method syntax
it.flaky('flaky test using method syntax', () => {
  flakyTestMethodInvocations++;
  if (flakyTestMethodInvocations < 2) {
    throw new Error('Simulated failure');
  }
  // Passes on 2nd attempt
});

// Test that always fails (exhausts retries)
test('flaky test that always fails', { flaky: 3 }, () => {
  flakyTestAlwaysFails++;
  throw new Error(`Always fails (attempt ${flakyTestAlwaysFails})`);
});

// Test with flaky: true (should use default 20)
test('flaky test with true value', { flaky: true }, () => {
  // This one passes immediately
});

// Suite with flaky option
describe.flaky('flaky suite', () => {
  it('inherits flaky from suite', () => {
    // Passes immediately
  });

  it('override flaky in test', { flaky: false }, () => {
    // Not flaky despite suite being flaky
  });
});

// Invalid flaky values should error
try {
  test('invalid flaky value -1', { flaky: -1 }, () => {});
} catch (err) {
  console.log('Expected error for negative flaky:', err.message);
}

try {
  test('invalid flaky value 0', { flaky: 0 }, () => {});
} catch (err) {
  console.log('Expected error for zero flaky:', err.message);
}

try {
  test('invalid flaky value string', { flaky: 'invalid' }, () => {});
} catch (err) {
  console.log('Expected error for string flaky:', err.message);
}
