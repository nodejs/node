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

var WINDOW = 200; // why is does this need to be so big?

var interval_count = 0;
var setTimeout_called = false;

// check that these don't blow up.
clearTimeout(null);
clearInterval(null);

assert.equal(true, setTimeout instanceof Function);
var starttime = new Date;
setTimeout(function() {
  var endtime = new Date;

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error('diff: ' + diff);

  assert.equal(true, 1000 - WINDOW < diff && diff < 1000 + WINDOW);
  setTimeout_called = true;
}, 1000);

// this timer shouldn't execute
var id = setTimeout(function() { assert.equal(true, false); }, 500);
clearTimeout(id);

setInterval(function() {
  interval_count += 1;
  var endtime = new Date;

  var diff = endtime - starttime;
  assert.ok(diff > 0);
  console.error('diff: ' + diff);

  var t = interval_count * 1000;

  assert.equal(true, t - WINDOW < diff && diff < t + WINDOW);

  assert.equal(true, interval_count <= 3);
  if (interval_count == 3)
    clearInterval(this);
}, 1000);


// Single param:
setTimeout(function(param) {
  assert.equal('test param', param);
}, 1000, 'test param');

var interval_count2 = 0;
setInterval(function(param) {
  ++interval_count2;
  assert.equal('test param', param);

  if (interval_count2 == 3)
    clearInterval(this);
}, 1000, 'test param');


// Multiple param
setTimeout(function(param1, param2) {
  assert.equal('param1', param1);
  assert.equal('param2', param2);
}, 1000, 'param1', 'param2');

var interval_count3 = 0;
setInterval(function(param1, param2) {
  ++interval_count3;
  assert.equal('param1', param1);
  assert.equal('param2', param2);

  if (interval_count3 == 3)
    clearInterval(this);
}, 1000, 'param1', 'param2');

// setInterval(cb, 0) should be called multiple times.
var count4 = 0;
var interval4 = setInterval(function() {
  if (++count4 > 10) clearInterval(interval4);
}, 0);


// we should be able to clearTimeout multiple times without breakage.
var expectedTimeouts = 3;

function t() {
  expectedTimeouts--;
}

var w = setTimeout(t, 200);
var x = setTimeout(t, 200);
var y = setTimeout(t, 200);

clearTimeout(y);
var z = setTimeout(t, 200);
clearTimeout(y);


process.addListener('exit', function() {
  assert.equal(true, setTimeout_called);
  assert.equal(3, interval_count);
  assert.equal(11, count4);
  assert.equal(0, expectedTimeouts, 'clearTimeout cleared too many timeouts');
});
