'use strict';

// Tests below are not from WPT.

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;

function makeIterableFunc(array) {
  return Object.assign(() => {}, {
    [Symbol.iterator]() {
      return array[Symbol.iterator]();
    }
  });
}

{
  const iterableError = common.expectsError({
    code: 'ERR_ARG_NOT_ITERABLE',
    type: TypeError,
    message: 'Query pairs must be iterable'
  });
  const tupleError = common.expectsError({
    code: 'ERR_INVALID_TUPLE',
    type: TypeError,
    message: 'Each query pair must be an iterable [name, value] tuple'
  }, 6);

  let params;
  params = new URLSearchParams(undefined);
  assert.strictEqual(params.toString(), '');
  params = new URLSearchParams(null);
  assert.strictEqual(params.toString(), '');
  params = new URLSearchParams(
    makeIterableFunc([['key', 'val'], ['key2', 'val2']])
  );
  assert.strictEqual(params.toString(), 'key=val&key2=val2');
  params = new URLSearchParams(
    makeIterableFunc([['key', 'val'], ['key2', 'val2']].map(makeIterableFunc))
  );
  assert.strictEqual(params.toString(), 'key=val&key2=val2');
  assert.throws(() => new URLSearchParams([[1]]), tupleError);
  assert.throws(() => new URLSearchParams([[1, 2, 3]]), tupleError);
  assert.throws(() => new URLSearchParams({ [Symbol.iterator]: 42 }),
                iterableError);
  assert.throws(() => new URLSearchParams([{}]), tupleError);
  assert.throws(() => new URLSearchParams(['a']), tupleError);
  assert.throws(() => new URLSearchParams([null]), tupleError);
  assert.throws(() => new URLSearchParams([{ [Symbol.iterator]: 42 }]),
                tupleError);
}

{
  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  const toStringError = /^Error: toString$/;
  const symbolError = /^TypeError: Cannot convert a Symbol value to a string$/;

  assert.throws(() => new URLSearchParams({ a: obj }), toStringError);
  assert.throws(() => new URLSearchParams([['a', obj]]), toStringError);
  assert.throws(() => new URLSearchParams(sym), symbolError);
  assert.throws(() => new URLSearchParams({ [sym]: 'a' }), symbolError);
  assert.throws(() => new URLSearchParams({ a: sym }), symbolError);
  assert.throws(() => new URLSearchParams([[sym, 'a']]), symbolError);
  assert.throws(() => new URLSearchParams([['a', sym]]), symbolError);
}
