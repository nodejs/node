// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { bench, createRunner, run } = require('node:bench');

const noop = () => {};
let functionOverloadCalls = 0;
let objectOverloadCalls = 0;

function functionOverload(b) {
  functionOverloadCalls++;
  completeSample(b);
  b.done();
}

function objectOverload(b) {
  objectOverloadCalls++;
  completeSample(b);
}

assert.throws(() => bench('', noop), { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => bench('name', null), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { samples: 0 }, noop),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => bench('name', { warmup: -1 }, noop),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => bench('name', { timeout: -1 }, noop),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => bench('name', { signal: {} }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { diagnosticChannels: 'channel' }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { diagnosticChannels: [1] }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { tags: 'fast' }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { tags: [''] }, noop),
              { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => bench('name', { params: { value: null } }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { params: { value: NaN } }, noop),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => bench('name', { only: 'yes' }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => bench('name', { skip: 1 }, noop),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => run({ namePattern: 1 }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => run({ samples: 0 }),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => run({ warmup: -1 }),
              { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => run({ yieldBetweenSamples: 1 }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => createRunner(null),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => createRunner({ yieldBetweenSamples: 1 }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => createRunner({ yieldBetweenSamples: null }),
              { code: 'ERR_INVALID_ARG_TYPE' });

bench(functionOverload);
bench({ samples: 1 }, objectOverload);

bench('valid', { samples: 1 }, (b) => {
  completeSample(b);
});

const stream = run();
stream.on('bench:start', common.mustCall(() => {
  assert.throws(() => bench('late', noop), { code: 'ERR_INVALID_STATE' });
}, 3));
stream.on('end', common.mustCall(() => {
  assert.strictEqual(functionOverloadCalls, 1);
  assert.strictEqual(objectOverloadCalls, 1);
}));
stream.resume();
