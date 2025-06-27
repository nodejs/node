'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;

if (process.argv[2] === 'child') {
  process.send('ok', common.mustCall((err) => {
    assert.strictEqual(err, null);
  }));
} else {
  const child = fork(process.argv[1], ['child']);
  child.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'ok');
  }));
  child.on('exit', common.mustCall((exitCode, signalCode) => {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signalCode, null);
  }));
}
