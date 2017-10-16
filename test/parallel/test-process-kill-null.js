'use strict';
const { mustCall } = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const cat = spawn('cat');

assert.ok(process.kill(cat.pid, 0));

cat.on('exit', mustCall(function() {
  assert.throws(function() {
    process.kill(cat.pid, 0);
  }, Error);
}));

cat.stdout.on('data', mustCall(function() {
  process.kill(cat.pid, 'SIGKILL');
}));

// EPIPE when null sig fails
cat.stdin.write('test');
