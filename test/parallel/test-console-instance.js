'use strict';
require('../common');
const assert = require('assert');
const Stream = require('stream');
const Console = require('console').Console;
var called = false;

const out = new Stream();
const err = new Stream();

// ensure the Console instance doesn't write to the
// process' "stdout" or "stderr" streams
process.stdout.write = process.stderr.write = function() {
  throw new Error('write() should not be called!');
};

// make sure that the "Console" function exists
assert.strictEqual('function', typeof Console);

// make sure that the Console constructor throws
// when not given a writable stream instance
assert.throws(function() {
  new Console();
}, /Console expects a writable stream/);

// Console constructor should throw if stderr exists but is not writable
assert.throws(function() {
  out.write = function() {};
  err.write = undefined;
  new Console(out, err);
}, /Console expects writable stream instances/);

out.write = err.write = function(d) {};

var c = new Console(out, err);

out.write = err.write = function(d) {
  assert.strictEqual(d, 'test\n');
  called = true;
};

assert(!called);
c.log('test');
assert(called);

called = false;
c.error('test');
assert(called);

out.write = function(d) {
  assert.strictEqual('{ foo: 1 }\n', d);
  called = true;
};

called = false;
c.dir({ foo: 1 });
assert(called);

// ensure that the console functions are bound to the console instance
called = 0;
out.write = function(d) {
  called++;
  assert.strictEqual(d, called + ' ' + (called - 1) + ' [ 1, 2, 3 ]\n');
};
[1, 2, 3].forEach(c.log);
assert.strictEqual(3, called);

// Console() detects if it is called without `new` keyword
assert.doesNotThrow(function() {
  Console(out, err);
});
