'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// Test that when a domain's error handler throws, the exception does NOT
// recursively call the same domain's error handler. Instead, it should
// propagate to the parent domain or crash the process.

let d1ErrorCount = 0;
let d2ErrorCount = 0;

const d1 = domain.create();
const d2 = domain.create();

d1.on('error', common.mustCall((err) => {
  d1ErrorCount++;
  assert.strictEqual(err.message, 'from d2 error handler');
  // d1 should only be called once - when d2's error handler throws
  assert.strictEqual(d1ErrorCount, 1);
}));

d2.on('error', common.mustCall((err) => {
  d2ErrorCount++;
  // d2's error handler should only be called once for the original error
  assert.strictEqual(d2ErrorCount, 1);
  assert.strictEqual(err.message, 'original error');
  // Throwing here should go to d1, NOT back to d2
  throw new Error('from d2 error handler');
}));

d1.run(() => {
  d2.run(() => {
    // Emit an error on an EventEmitter bound to d2
    const EventEmitter = require('events');
    const ee = new EventEmitter();
    d2.add(ee);

    // This should:
    // 1. Call d2's error handler with 'original error'
    // 2. d2's error handler throws 'from d2 error handler'
    // 3. That exception should go to d1's error handler, NOT d2 again
    ee.emit('error', new Error('original error'));
  });
});
