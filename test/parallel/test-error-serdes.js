// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;
const { serializeError, deserializeError } = require('internal/error_serdes');

function cycle(err) {
  return deserializeError(serializeError(err));
}

assert.strictEqual(cycle(0), 0);
assert.strictEqual(cycle(-1), -1);
assert.strictEqual(cycle(1.4), 1.4);
assert.strictEqual(cycle(null), null);
assert.strictEqual(cycle(undefined), undefined);
assert.strictEqual(cycle('foo'), 'foo');

let err = new Error('foo');
for (let i = 0; i < 10; i++) {
  assert(err instanceof Error);
  assert(Object.prototype.toString.call(err), '[object Error]');
  assert.strictEqual(err.name, 'Error');
  assert.strictEqual(err.message, 'foo');
  assert(/^Error: foo\n/.test(err.stack));

  const prev = err;
  err = cycle(err);
  assert.deepStrictEqual(err, prev);
}

assert.strictEqual(cycle(new RangeError('foo')).name, 'RangeError');
assert.strictEqual(cycle(new TypeError('foo')).name, 'TypeError');
assert.strictEqual(cycle(new ReferenceError('foo')).name, 'ReferenceError');
assert.strictEqual(cycle(new URIError('foo')).name, 'URIError');
assert.strictEqual(cycle(new EvalError('foo')).name, 'EvalError');
assert.strictEqual(cycle(new SyntaxError('foo')).name, 'SyntaxError');

class SubError extends Error {}

assert.strictEqual(cycle(new SubError('foo')).name, 'Error');

assert.deepStrictEqual(cycle({ message: 'foo' }), { message: 'foo' });
assert.strictEqual(cycle(Function), '[Function: Function]');

{
  const err = new ERR_INVALID_ARG_TYPE('object', 'Object', 42);
  assert(/^TypeError \[ERR_INVALID_ARG_TYPE\]:/.test(err));
  assert.strictEqual(err.name, 'TypeError');
  assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
}

{
  let called = false;
  class DynamicError extends Error {
    get type() {
      called = true;
      return 'dynamic';
    }

    get shouldIgnoreError() {
      throw new Error();
    }
  }

  serializeError(new DynamicError());
  assert.deepStrictEqual(called, true);
}
