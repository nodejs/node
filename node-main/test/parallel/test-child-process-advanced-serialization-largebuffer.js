'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

// Regression test for https://github.com/nodejs/node/issues/34797
const eightMB = 8 * 1024 * 1024;

if (process.argv[2] === 'child') {
  for (let i = 0; i < 4; i++) {
    process.send(new Uint8Array(eightMB).fill(i));
  }
} else {
  const child = child_process.spawn(process.execPath, [__filename, 'child'], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    serialization: 'advanced'
  });
  const received = [];
  child.on('message', common.mustCall((chunk) => {
    assert.deepStrictEqual(chunk, new Uint8Array(eightMB).fill(chunk[0]));

    received.push(chunk[0]);
    if (received.length === 4) {
      assert.deepStrictEqual(received, [0, 1, 2, 3]);
    }
  }, 4));
}
