'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;

if (process.argv[2] === 'child') {
  process.send('ok', common.mustCall(function(err) {
    assert.strictEqual(err, null);
  }));
} else {
  const child = fork(process.argv[1], ['child']);
  child.on('message', common.mustCall(function(message) {
    assert.strictEqual(message, 'ok');
  }));
  child.on('exit', common.mustCall(function(exitCode, signalCode) {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signalCode, null);
  }));
}
