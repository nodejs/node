'use strict';

require('../common');
const vm = require('vm');
const assert = require('assert');

// Each [[DefineOwnProperty]] intercepted by the definer should invoke the
// sandbox's [[DefineOwnProperty]] exactly once.
{
  let count = 0;
  const sandbox = new Proxy({}, {
    defineProperty(target, key, desc) {
      count++;
      return Reflect.defineProperty(target, key, desc);
    },
  });
  const ctx = vm.createContext(sandbox);
  vm.runInContext(`
    Object.defineProperty(this, 'a', { value: 1 });
    Object.defineProperty(this, 'b', { value: 2, writable: true });
    Object.defineProperty(this, 'c', { get() { return 3; } });
  `, ctx);
  assert.strictEqual(count, 3);
}
