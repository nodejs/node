// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const sandbox = { x: 3 };

const ctx = vm.createContext(sandbox);

assert.strictEqual(vm.runInContext('x;', ctx), 3);
vm.runInContext('y = 4;', ctx);
assert.strictEqual(sandbox.y, 4);
assert.strictEqual(ctx.y, 4);

// Test `IndexedPropertyGetterCallback` and `IndexedPropertyDeleterCallback`
const x = { get 1() { return 5; } };
const pd_expected = Object.getOwnPropertyDescriptor(x, 1);
const ctx2 = vm.createContext(x);
const pd_actual = Object.getOwnPropertyDescriptor(ctx2, 1);

assert.deepStrictEqual(pd_actual, pd_expected);
assert.strictEqual(ctx2[1], 5);
delete ctx2[1];
assert.strictEqual(ctx2[1], undefined);

// https://github.com/nodejs/node/issues/33806
{
  const ctx = vm.createContext();

  Object.defineProperty(ctx, 'prop', {
    get() {
      return undefined;
    },
    set(val) {
      throw new Error('test error');
    },
  });

  assert.throws(() => {
    vm.runInContext('prop = 42', ctx);
  }, {
    message: 'test error',
  });
}
