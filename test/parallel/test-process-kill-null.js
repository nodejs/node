'use strict';
require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var cat = spawn('cat');
var called;

assert.ok(process.kill(cat.pid, 0));

cat.on('exit', function() {
  assert.throws(function() {
    process.kill(cat.pid, 0);
  }, Error);
});

cat.stdout.on('data', function() {
  called = true;
  process.kill(cat.pid, 'SIGKILL');
});

// EPIPE when null sig fails
cat.stdin.write('test');

process.on('exit', function() {
  assert.ok(called);
});
