'use strict';

// This tests that mask > 0o777 will be masked off with 0o777 in
// process.umask()

const common = require('../common');
const assert = require('assert');

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
