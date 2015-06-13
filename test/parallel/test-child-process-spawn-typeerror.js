'use strict';
var assert = require('assert');
var child_process = require('child_process');
var spawn = child_process.spawn;
var cmd = (process.platform === 'win32') ? 'rundll32' : 'ls';
var invalidArgsMsg = /Incorrect value of args option/;
var invalidOptionsMsg = /options argument must be an object/;

// verify that args argument must be an array
assert.throws(function() {
  spawn(cmd, 'this is not an array');
}, TypeError);

// verify that valid argument combinations do not throw
assert.doesNotThrow(function() {
  spawn(cmd);
});

assert.doesNotThrow(function() {
  spawn(cmd, []);
});

assert.doesNotThrow(function() {
  spawn(cmd, {});
});

assert.doesNotThrow(function() {
  spawn(cmd, [], {});
});

// verify that invalid argument combinations throw
assert.throws(function() {
  spawn();
}, /Bad argument/);

assert.throws(function() {
  spawn(cmd, null);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, true);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, [], null);
}, invalidOptionsMsg);

assert.throws(function() {
  spawn(cmd, [], 1);
}, invalidOptionsMsg);

