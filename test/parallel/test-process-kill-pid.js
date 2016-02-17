'use strict';
require('../common');
var assert = require('assert');

// test variants of pid
//
// null: TypeError
// undefined: TypeError
//
// 'SIGTERM': TypeError
//
// String(process.pid): TypeError
//
// Nan, Infinity, -Infinity: TypeError
//
// 0, String(0): our group process
//
// process.pid, String(process.pid): ourself

assert.throws(function() { process.kill('SIGTERM'); }, TypeError);
assert.throws(function() { process.kill(null); }, TypeError);
assert.throws(function() { process.kill(undefined); }, TypeError);
assert.throws(function() { process.kill(+'not a number'); }, TypeError);
assert.throws(function() { process.kill(1 / 0); }, TypeError);
assert.throws(function() { process.kill(-1 / 0); }, TypeError);

// Test kill argument processing in valid cases.
//
// Monkey patch _kill so that we don't actually send any signals, particularly
// that we don't kill our process group, or try to actually send ANY signals on
// windows, which doesn't support them.
function kill(tryPid, trySig, expectPid, expectSig) {
  var getPid;
  var getSig;
  var origKill = process._kill;
  process._kill = function(pid, sig) {
    getPid = pid;
    getSig = sig;

    // un-monkey patch process._kill
    process._kill = origKill;
  };

  process.kill(tryPid, trySig);

  assert.equal(getPid, expectPid);
  assert.equal(getSig, expectSig);
}

// Note that SIGHUP and SIGTERM map to 1 and 15 respectively, even on Windows
// (for Windows, libuv maps 1 and 15 to the correct behaviour).

kill(0, 'SIGHUP', 0, 1);
kill(0, undefined, 0, 15);
kill('0', 'SIGHUP', 0, 1);
kill('0', undefined, 0, 15);

// negative numbers are meaningful on unix
kill(-1, 'SIGHUP', -1, 1);
kill(-1, undefined, -1, 15);
kill('-1', 'SIGHUP', -1, 1);
kill('-1', undefined, -1, 15);

kill(process.pid, 'SIGHUP', process.pid, 1);
kill(process.pid, undefined, process.pid, 15);
kill(String(process.pid), 'SIGHUP', process.pid, 1);
kill(String(process.pid), undefined, process.pid, 15);
