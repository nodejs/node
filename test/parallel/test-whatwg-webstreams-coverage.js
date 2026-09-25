// Flags: --no-warnings --expose-internals
'use strict';

require('../common');

const {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
} = require('stream/web');

const {
  inspect,
} = require('util');

const {
  isPromisePending,
} = require('internal/webstreams/util');

const assert = require('assert');

assert(!isPromisePending({}));
assert(!isPromisePending(Promise.resolve()));
assert(isPromisePending(new Promise(() => {})));

// Brand checking works
assert.throws(() => {
  Reflect.get(ByteLengthQueuingStrategy.prototype, 'highWaterMark', {});
}, {
  name: 'TypeError',
  message: /Cannot read private member/,
});

assert.throws(() => {
  Reflect.get(ByteLengthQueuingStrategy.prototype, 'size', {});
}, {
  name: 'TypeError',
  message: /Cannot read private member/,
});

assert.throws(() => {
  Reflect.get(CountQueuingStrategy.prototype, 'highWaterMark', {});
}, {
  name: 'TypeError',
  message: /Cannot read private member/,
});

assert.throws(() => {
  Reflect.get(CountQueuingStrategy.prototype, 'size', {});
}, {
  name: 'TypeError',
  message: /Cannot read private member/,
});

// Custom Inspect Works

{
  const strategy = new CountQueuingStrategy({ highWaterMark: 1 });

  assert.strictEqual(
    inspect(strategy, { depth: null }),
    'CountQueuingStrategy { highWaterMark: 1 }');

  assert.strictEqual(
    inspect(strategy),
    'CountQueuingStrategy { highWaterMark: 1 }');

  assert.strictEqual(
    inspect(strategy, { depth: 0 }),
    'CountQueuingStrategy [Object]');

  assert.strictEqual(
    inspect(new ByteLengthQueuingStrategy({ highWaterMark: 1 })),
    'ByteLengthQueuingStrategy { highWaterMark: 1 }');
}
