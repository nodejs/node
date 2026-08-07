'use strict';
const common = require('../common');

if (!common.isMainThread)
  common.skip('process.stdout is not a tty in Workers');

const assert = require('assert');
const tty = require('tty');

// Placeholder test: just verifies the API surface exists and returns
// a Promise. Real OSC 11 protocol/timeout tests will be added once
// the implementation is complete.
const stream = new tty.WriteStream(1);

assert.strictEqual(typeof stream.getBackgroundColor, 'function');

const result = stream.getBackgroundColor();
assert.ok(result instanceof Promise);