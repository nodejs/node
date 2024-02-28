'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => structuredClone(), { code: 'ERR_MISSING_ARGS' });
assert.throws(() => structuredClone(undefined, ''), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, 1), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, { transfer: 1 }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, { transfer: '' }), { code: 'ERR_INVALID_ARG_TYPE' });

// Options can be null or undefined.
assert.strictEqual(structuredClone(undefined), undefined);
assert.strictEqual(structuredClone(undefined, null), undefined);
// Transfer can be null or undefined.
assert.strictEqual(structuredClone(undefined, { transfer: null }), undefined);
assert.strictEqual(structuredClone(undefined, { }), undefined);

{
  // See: https://github.com/nodejs/node/issues/49940
  const cloned = structuredClone({}, {
    transfer: {
      *[Symbol.iterator]() {}
    }
  });

  assert.deepStrictEqual(cloned, {});
}
