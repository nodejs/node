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
const { tagLFy } = require('../common');
const assert = require('assert');

const vm = require('vm');

const code = tagLFy`
    Object.defineProperty(this, 'f', {
      get: function() { return x; },
      set: function(k) { x = k; },
      configurable: true,
      enumerable: true
    });
    g = f;
    f;
`;

const x = {};
const o = vm.createContext({ console: console, x: x });

const res = vm.runInContext(code, o, 'test');

assert(res);
assert.strictEqual(typeof res, 'object');
assert.strictEqual(res, x);
assert.strictEqual(o.f, res);
assert.deepStrictEqual(Object.keys(o), ['console', 'x', 'g', 'f']);
