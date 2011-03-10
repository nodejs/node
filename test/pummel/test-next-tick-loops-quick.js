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

var common = require('../common');
var assert = require('assert');

// Regression test GH-511: https://github.com/ry/node/issues/issue/511
// Make sure nextTick loops quickly

setTimeout(function () {
 t = Date.now() - t;
 STOP = 1;
 console.log(["ctr: ",ctr, ", t:", t, "ms -> ", (ctr/t).toFixed(2), "KHz"].join(''));
 assert.ok(ctr > 1000);
}, 2000);

var ctr = 0;
var STOP = 0;
var t = Date.now()+ 2;
while (t > Date.now()) ; //get in sync with clock

(function foo () {
  if (STOP) return;
  process.nextTick(foo);
  ctr++;
})();
