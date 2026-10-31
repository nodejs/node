'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

// Errors thrown by accessors on a contextified global must propagate.
{
  const context = vm.createContext({});
  const result = vm.runInContext(`
    let getterCalls = 0;
    Object.defineProperty(globalThis, 'prop', {
      configurable: true,
      get() {
        getterCalls++;
        throw new Error('getter failure');
      },
    });
    let message;
    try {
      globalThis.prop;
    } catch (err) {
      message = err.message;
    }
    ({ message, getterCalls });
  `, context);
  assert.deepStrictEqual({ ...result }, {
    message: 'getter failure',
    getterCalls: 1,
  });
}

{
  const sandbox = {};
  Object.defineProperty(sandbox, 'prop', {
    get() {
      throw new Error('sandbox getter failure');
    },
  });
  const context = vm.createContext(sandbox);
  assert.throws(() => vm.runInContext('prop', context), {
    message: 'sandbox getter failure',
  });
  assert.throws(() => vm.runInContext('globalThis.prop', context), {
    message: 'sandbox getter failure',
  });
}
