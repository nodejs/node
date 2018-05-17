'use strict';

// This tests that the lower bits of mode > 0o777 still works in
// process.umask()

const common = require('../common');
const assert = require('assert');

if (!common.isMainThread)
  common.skip('process.umask is not available in Workers');

let mask;

if (common.isWindows) {
  mask = 0o600;
} else {
  mask = 0o664;
}

const maskToIgnore = 0o10000;

const old = process.umask();

function test(input, output) {
  process.umask(input);
  assert.strictEqual(process.umask(), output);

  process.umask(old);
}

test(mask | maskToIgnore, mask);
test((mask | maskToIgnore).toString(8), mask);
