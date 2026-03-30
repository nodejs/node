'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const NUM_MESSAGES = 10;
const values = [];

for (let i = 0; i < NUM_MESSAGES; ++i) {
  values[i] = i;
}

if (process.argv[2] === 'child') {
  const received = values.map(() => { return false; });

  process.on('uncaughtException', common.mustCall((err) => {
    received[err] = true;
    const done = received.every((element) => { return element === true; });

    if (done)
      process.disconnect();
  }, NUM_MESSAGES));

  process.on('message', (msg) => {
    // If messages are handled synchronously, throwing should break the IPC
    // message processing.
    throw msg;
  });

  process.send('ready');
} else {
  const child = cp.fork(__filename, ['child']);

  child.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'ready');
    values.forEach((value) => {
      child.send(value);
    });
  }));
}
