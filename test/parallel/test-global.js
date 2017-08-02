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

// This test cannot run in strict mode because it tests that `baseFoo` is
// treated as a global without being declared with `var`/`let`/`const`.

/* eslint-disable strict */
const common = require('../common');

const assert = require('assert');
const path = require('path');

common.globalCheck = false;

baseFoo = 'foo'; // eslint-disable-line no-undef
global.baseBar = 'bar';

assert.strictEqual(global.baseFoo, 'foo',
                   'x -> global.x in base level not working');

assert.strictEqual(baseBar, // eslint-disable-line no-undef
                   'bar',
                   'global.x -> x in base level not working');

const mod = require(path.join(common.fixturesDir, 'global', 'plain'));
const fooBar = mod.fooBar;

assert.strictEqual(fooBar.foo, 'foo', 'x -> global.x in sub level not working');

assert.strictEqual(fooBar.bar, 'bar', 'global.x -> x in sub level not working');

assert.strictEqual(Object.prototype.toString.call(global), '[object global]');
