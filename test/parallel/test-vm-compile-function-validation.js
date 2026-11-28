'use strict';

// Refs: https://github.com/nodejs/node/issues/67366

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

assert.throws(() => {
  vm.compileFunction('return 1', [], { contextExtensions: [null] });
}, {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "options.contextExtensions[0]" property must be of type ' +
    'object. Received null',
});

assert.throws(() => {
  vm.compileFunction('return 1', [], { contextExtensions: [{}, null] });
}, {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "options.contextExtensions[1]" property must be of type ' +
    'object. Received null',
});

{
  let reads = 0;
  const params = [];
  Object.defineProperty(params, 0, {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get: common.mustCall(() => (++reads === 1 ? 'x' : 42)),
  });

  const fn = vm.compileFunction('return x', params);
  assert.strictEqual(fn(5), 5);
}

{
  let reads = 0;
  const contextExtensions = [];
  Object.defineProperty(contextExtensions, 0, {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get: common.mustCall(() => (++reads === 1 ? { a: 5 } : null)),
  });

  const fn = vm.compileFunction('return a', [], { contextExtensions });
  assert.strictEqual(fn(), 5);
}

{
  const params = [];
  Object.defineProperty(params, 0, {
    __proto__: null,
    enumerable: true,
    configurable: true,
    get: common.mustCall(() => 42),
  });

  assert.throws(() => vm.compileFunction('return 1', params), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "params[0]" argument must be of type string. ' +
      'Received type number (42)',
  });
}

{
  let reads = 0;
  const params = new Proxy(['x'], {
    get(target, key, receiver) {
      if (key === '0') return ++reads === 1 ? 'x' : 42;
      return Reflect.get(target, key, receiver);
    },
  });

  const fn = vm.compileFunction('return x', params);
  assert.strictEqual(fn(7), 7);
  assert.strictEqual(reads, 1);
}
