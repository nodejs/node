// Flags: --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { bench, run } = require('node:bench');

const noop = () => {};

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

bench('valid', { samples: 1 }, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});

const stream = run();
stream.on('bench:start', common.mustCall(() => {
  assert.throws(() => bench('late', noop), { code: 'ERR_INVALID_STATE' });
}));
stream.resume();
