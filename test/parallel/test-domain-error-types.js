// Flags: --gc-interval=100 --stress-compaction
'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// This test is similar to test-domain-multiple-errors, but uses a new domain
// for each errors.
// The test flags are not essential, but serve as a way to verify that
// https://github.com/nodejs/node/issues/28275 is fixed in debug mode.

for (const something of [
  42, null, undefined, false, () => {}, 'string', Symbol('foo'),
]) {
  const d = new domain.Domain();
  d.run(common.mustCall(() => {
    process.nextTick(common.mustCall(() => {
      throw something;
    }));
  }));

  d.on('error', common.mustCall((err) => {
    assert.strictEqual(something, err);
  }));
}
