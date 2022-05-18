'use strict';
const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');

// Callback gets "error" when first two args are the same
function cbError(arg1, arg2) {
  return arg1 === arg2 ? { code: 'CbError' } : null;
}

const cb = {
  classic(arg1, arg2, arg3, callback) {
    callback(cbError(arg1, arg2), arg3);
  },
  smart(arg1, arg2, arg3, callback) {
    // Try penultimate argument if the last is not set
    callback ??= arg3;
    callback(cbError(arg1, arg2), arg3);
  },
  returnMultiple(arg1, arg2, arg3, callback) {
    callback(cbError(arg1, arg2), arg1, arg2, arg3);
  },
  callbackFirst(callback, arg1 = 'default1', arg2 = 'default2', arg3 = 'default3') {
    callback(cbError(arg1, arg2), arg3);
  },
  callbackSecond(arg1, callback, arg2 = 'default2', arg3 = 'default3') {
    callback(cbError(arg1, arg2), arg3);
  },
  rest(callback, ...args) {
    callback(cbError(args[0], args[1]), args[2]);
  },
  returnRest(callback, ...args) {
    callback(cbError(args[0], args[1]), ...args);
  },
  hybrid(arg1, arg2, callback, ...args) {
    callback(cbError(arg1, arg2), args.length);
  },
  returnHybrid(arg1, arg2, callback, ...args) {
    callback(cbError(arg1, arg2), ...args);
  },
};

// Test that function name and length are always retained
Object.entries(cb).forEach(([fnName, fn]) => {
  const promisifiedFn = promisify(fn);
  assert.strictEqual(fnName, promisifiedFn.name);
  assert.strictEqual(fn.name, promisifiedFn.name);
  assert.strictEqual(fn.length, promisifiedFn.length);
});

// Tests for invalid numbers
{
  [
    NaN, -Infinity, -1, 1.5, 4, Infinity,
  ].forEach((invalidCallbackPosition) => {
    assert.rejects(
      promisify(cb.classic, { callbackPosition: invalidCallbackPosition })(1, 2, 3),
      { code: 'ERR_OUT_OF_RANGE' }
    );
  });
}

// Various tests
(async () => {
  assert.strictEqual(
    await promisify(cb.classic)(1, 2, 3),
    3
  );
  assert.strictEqual(
    await promisify(cb.classic, { callbackPosition: 3 })(1, 2, 3),
    3
  );
  assert.deepStrictEqual(
    await promisify(cb.classic, { resolveArray: true })(1, 2, 3),
    [3]
  );
  assert.deepStrictEqual(
    await promisify(cb.classic, { resolveObject: ['kFoo'] })(1, 2, 3),
    { kFoo: 3 }
  );
  assert.rejects(
    promisify(cb.classic)(1, 1, 3),
    { code: 'CbError' }
  );
  assert.rejects(
    promisify(cb.classic)(1, 2),
    TypeError
  );

  assert.strictEqual(
    await promisify(cb.smart)(1, 2, 3),
    3
  );
  assert.strictEqual(
    typeof await promisify(cb.smart)(1, 2),
    'function'
  );
  assert.strictEqual(
    await promisify(cb.smart, { callbackPosition: 3 })(1, 2, 3),
    3
  );
  assert.strictEqual(
    typeof await promisify(cb.smart, { callbackPosition: 2 })(1, 2),
    'function'
  );
  assert.rejects(
    promisify(cb.smart)(1, 1, 3),
    { code: 'CbError' }
  );
  assert.rejects(
    promisify(cb.smart, { callbackPosition: 3 })(1, 2),
    { code: 'ERR_OUT_OF_RANGE' }
  );
  assert.rejects(
    promisify(cb.smart, { callbackPosition: 2 })(1, 2, 3),
    TypeError
  );

  assert.strictEqual(
    await promisify(cb.returnMultiple, { resolveArray: false })(1, 2, 3),
    1
  );
  assert.deepStrictEqual(
    await promisify(cb.returnMultiple, { resolveArray: true })(1, 2, 3),
    [1, 2, 3]
  );
  assert.deepStrictEqual(
    await promisify(cb.returnMultiple, { resolveObject: ['kFoo', 'kBar', 'kBaz'] })(1, 2, 3),
    { kFoo: 1, kBar: 2, kBaz: 3 }
  );

  assert.strictEqual(
    await promisify(cb.callbackFirst, { callbackPosition: 0 })(1, 2, 3),
    3
  );
  assert.strictEqual(
    await promisify(cb.callbackFirst, { callbackPosition: 0 })(1, 2),
    'default3'
  );

  assert.strictEqual(
    await promisify(cb.callbackSecond, { callbackPosition: 1 })(1, 2, 3),
    3
  );
  assert.strictEqual(
    await promisify(cb.callbackSecond, { callbackPosition: 1 })(1, 2),
    'default3'
  );

  assert.strictEqual(
    await promisify(cb.rest, { callbackPosition: 0 })(1, 2, 3, 4),
    3
  );
  assert.rejects(
    promisify(cb.rest, { callbackPosition: 0 })(1, 1, 3, 4),
    { code: 'CbError' }
  );
  assert.strictEqual(
    await promisify(cb.rest, { callbackPosition: 0 })(1, 2),
    undefined
  );

  assert.deepStrictEqual(
    await promisify(cb.returnRest, { callbackPosition: 0, resolveArray: true })(1, 2, 3, 4, 5),
    [1, 2, 3, 4, 5]
  );
  assert.deepStrictEqual(
    await promisify(cb.returnRest, {
      callbackPosition: 0,
      resolveObject: ['a', 'b', 'c', 'd', 'e']
    })(1, 2, 3, 4, 5),
    { a: 1, b: 2, c: 3, d: 4, e: 5 }
  );
  assert.deepStrictEqual(
    await promisify(cb.returnRest, {
      callbackPosition: 0,
      resolveObject: ['a', 'b', 'c', 'd', 'e', 'f'],
    })(1, 2, 3, 4, 5),
    { a: 1, b: 2, c: 3, d: 4, e: 5, f: undefined }
  );

  assert.strictEqual(
    await promisify(cb.hybrid, { callbackPosition: 2 })(1, 2, 3, 4, 5, 6),
    4
  );

  assert.deepStrictEqual(
    await promisify(cb.returnHybrid, { callbackPosition: 2, resolveArray: true })(1, 2, 3, 4, 5, 6),
    [3, 4, 5, 6]
  );
  assert.deepStrictEqual(
    await promisify(cb.returnHybrid, {
      callbackPosition: 2,
      resolveObject: ['a', 'b', 'c', 'd', 'e', 'f']
    })(1, 2, 3, 4, 5, 6),
    { a: 3, b: 4, c: 5, d: 6, e: undefined, f: undefined }
  );
})().then(common.mustCall());
