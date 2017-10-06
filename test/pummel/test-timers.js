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

const WINDOW = 200; // why is does this need to be so big?

let interval_count = 0;

// check that these don't blow up.
clearTimeout(null);
clearInterval(null);

assert.strictEqual(true, setTimeout instanceof Function);
const starttime = new Date();
setTimeout(common.mustCall(function() {
  const endtime = new Date();

  const diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error(`diff: ${diff}`);

  assert.strictEqual(true, 1000 - WINDOW < diff && diff < 1000 + WINDOW);
}), 1000);

// this timer shouldn't execute
const id = setTimeout(function() { assert.strictEqual(true, false); }, 500);
clearTimeout(id);

setInterval(function() {
  interval_count += 1;
  const endtime = new Date();

  const diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error(`diff: ${diff}`);

  const t = interval_count * 1000;

  assert.strictEqual(true, t - WINDOW < diff && diff < t + WINDOW);

  assert.strictEqual(true, interval_count <= 3);
  if (interval_count === 3)
    clearInterval(this);
}, 1000);


// Single param:
let stpOne = 'test param';
setTimeout(function(param) {
  assert.strictEqual(stpOne, param, `setTimeout: passing single argument param differs, passed "${stpOne}" , got "${param}"`);
}, 1000, stpOne);

let interval_count2 = 0;
let sipOne = 'test param';
setInterval(function(param) {
  ++interval_count2;
  assert.strictEqual(sipOne, param, `setInterval: passing single argument param differs, passed "${sipOne}" , got "${param}" at iteration ${interval_count2 - 1}`);

  if (interval_count2 === 3)
    clearInterval(this);
}, 1000, sipOne);


// Multiple param
let stpmOne = 'param1', stpmTwo = 'param2';
setTimeout(function(param1, param2) {
  assert.strictEqual(stpmOne, param1, `setInterval: passing two argument first parameter differs, passed "${stpmOne}", got "${param1}"`);
  assert.strictEqual(stpmTwo, param2, `setInterval: passing two argument second parameter differs, passed "${stpmTwo}", got "${param2}"`);
}, 1000, stpmOne, stpmTwo);

let interval_count3 = 0;
let sipmOne = 'param1', sipmTwo = 'param2';
setInterval(function(param1, param2) {
  ++interval_count3;
  assert.strictEqual(sipmOne, param1, `setInterval: passing two argument first parameter differs, passed "${sipmOne}", got "${param1}" at iteration ${interval_count3 - 1}`);
  assert.strictEqual(sipmTwo, param2, `setInterval: passing two argument second parameter differs, passed "${sipmTwo}", got "${param2}" at iteration ${interval_count3 - 1}`);

  if (interval_count3 === 3)
    clearInterval(this);
}, 1000, sipmOne, sipmTwo);

// setInterval(cb, 0) should be called multiple times.
let count4 = 0;
const interval4 = setInterval(function() {
  if (++count4 > 10) clearInterval(interval4);
}, 0);


// we should be able to clearTimeout multiple times without breakage.
let expectedTimeouts = 3;

function t() {
  expectedTimeouts--;
}

setTimeout(t, 200);
setTimeout(t, 200);
const y = setTimeout(t, 200);

clearTimeout(y);
setTimeout(t, 200);
clearTimeout(y);


process.on('exit', function() {
  assert.strictEqual(3, interval_count);
  assert.strictEqual(11, count4);
  assert.strictEqual(0, expectedTimeouts,
                     'clearTimeout cleared too many timeouts');
});
