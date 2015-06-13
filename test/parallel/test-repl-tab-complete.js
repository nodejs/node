'use strict';
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
  };
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
testMe.complete('console.lo', function(error, data) {
  assert.deepEqual(data, [['console.log'], 'console.lo']);
});

// Tab Complete will return globaly scoped variables
putIn.run(['};']);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// Tab Complete will not break in an ternary operator with ()
putIn.run([
  'var inner = ( true ',
  '?',
  '{one: 1} : '
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, doesNotBreak);
});

putIn.run(['.clear']);

// Tab Complete will return a simple local variable
putIn.run([
  'var top = function() {',
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
  'var top = function() {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', function(error, data) {
  assert.deepEqual(data, works);
});

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the function
// has parameters
putIn.run([
  'var top = function(one, two) {',
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
  'var top = function() {',
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
// def has the params and { on a separate line
putIn.run([
  'var top = function() {',
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
  'var top = function() {',
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
  'var top = function() {',
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
  assert.deepEqual(data, [[], undefined]);
  clearTimeout(spaceTimeout);
});

// tab completion should pick up the global "toString" object, and
// any other properties up the "global" object's prototype chain
testMe.complete('toSt', function(error, data) {
  assert.deepEqual(data, [['toString'], 'toSt']);
});

// Tab complete provides built in libs for require()
putIn.run(['.clear']);

testMe.complete('require(\'', function(error, data) {
  assert.strictEqual(error, null);
  repl._builtinLibs.forEach(function(lib) {
    assert.notStrictEqual(data[0].indexOf(lib), -1, lib + ' not found');
  });
});

testMe.complete('require(\'n', function(error, data) {
  assert.strictEqual(error, null);
  assert.deepEqual(data, [['net'], 'n']);
});
