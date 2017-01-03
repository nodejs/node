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
const common = require('../common');
const assert = require('assert');

let immediateC;
let immediateD;

let mainFinished = false;

setImmediate(common.mustCall(function() {
  assert.strictEqual(mainFinished, true);
  clearImmediate(immediateB);
}));

const immediateB = setImmediate(common.mustNotCall());

setImmediate(function(x, y, z) {
  immediateC = [x, y, z];
}, 1, 2, 3);

setImmediate(function(x, y, z, a, b) {
  immediateD = [x, y, z, a, b];
}, 1, 2, 3, 4, 5);

process.on('exit', function() {
  assert.deepStrictEqual(immediateC, [1, 2, 3], 'immediateC args should match');
  assert.deepStrictEqual(immediateD, [1, 2, 3, 4, 5], '5 args should match');
});

mainFinished = true;
