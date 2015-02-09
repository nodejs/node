var common = require('../common');
var assert = require('assert');
var fork = require('child_process').fork;
var N = 4 << 20;  // 4 MB

for (var big = '*'; big.length < N; big += big);

if (process.argv[2] === 'child') {
  process.send(big);
  process.exit(42);
}

var proc = fork(__filename, ['child']);

proc.on('message', common.mustCall(function(msg) {
  assert.equal(typeof msg, 'string');
  assert.equal(msg.length, N);
  assert.equal(msg, big);
}));

proc.on('exit', common.mustCall(function(exitCode) {
  assert.equal(exitCode, 42);
}));
