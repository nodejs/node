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
var util = require('util');

var repl = require('repl');

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\n');
    });
  }
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

var works = [['inner.one'], 'inner.o'];
var doesNotBreak = [[], 'inner.o'];

var putIn = new ArrayStream();
var testMe = repl.start('', putIn);

// Tab Complete will not break in an object literal
putIn.run(['.clear']);
putIn.run([
  'var inner = {',
  'one:1'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

// Tab Complete will return globaly scoped variables
putIn.run(['};']);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// Tab Complete will not break in an ternary operator with ()
putIn.run([
  'var inner = ( true ' ,
  '?',
  '{one: 1} : '
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// Tab Complete will return a simple local variable
putIn.run([
  'var top = function () {',
  'var inner = {one:1};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

// When you close the function scope tab complete will not return the
// locally scoped variable
putIn.run(['};']);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// Tab Complete will return a complex local variable
putIn.run([
  'var top = function () {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the function
// has paramaters
putIn.run([
  'var top = function (one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the
// scope is nested inside an immediately executed function
putIn.run([
  'var top = function () {',
  '(function test () {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// currently does not work, but should not break note the inner function
// def has the params and { on a seperate line
putIn.run([
  'var top = function () {',
  'r = function test (',
  ' one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// currently does not work, but should not break, not the {
putIn.run([
  'var top = function () {',
  'r = function test ()',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// currently does not work, but should not break
putIn.run([
  'var top = function () {',
  'r = function test (',
  ')',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// make sure tab completion works on non-Objects
putIn.run([
  'var str = "test";'
]);
testMe.complete('str.len', function(error, data) {
  assert.deepEqual(data, [['str.length'], 'str.len']);
});

putIn.run(['.clear']);

// tab completion should not break on spaces
var spaceTimeout = setTimeout(function() {
  throw new Error('timeout');
}, 1000);

testMe.complete(' ', function(error, data) {
  assert.deepEqual(data, [[],undefined]);
  clearTimeout(spaceTimeout);
});

// tab completion should pick up the global "toString" object, and
// any other properties up the "global" object's prototype chain
testMe.complete('toSt', function(error, data) {
  assert.deepEqual(data, [['toString'], 'toSt']);
});
