'use strict';
// Refs: https://github.com/nodejs/node/issues/947
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  process.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'go');
    console.log('logging should not cause a crash');
    process.disconnect();
  }));
} else {
  const child = cp.fork(__filename, ['child'], {silent: true});

  child.on('close', common.mustCall((exitCode, signal) => {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signal, null);
  }));

  child.stdout.destroy();
  child.send('go');
}
