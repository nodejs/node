'use strict';
const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const fs = require('fs');

if (process.argv[2] === 'child-buffer') {
  const v = process.argv[3];
  const payload = Buffer.from([
    (v >> 24) & 0xFF,
    (v >> 16) & 0xFF,
    (v >> 8) & 0xFF,
    v & 0xFF,
  ]);
  const fd = process.channel?.fd;
  if (fd !== undefined) {
    fs.writeSync(fd, payload);
  }
  return;
}

const testCases = [
  0x00000001,
  0x7fffffff,
  0x80000000,
  0x80000001,
  0xffffffff,
];

for (const size of testCases) {
  const child = fork(__filename, ['child-buffer', size], {
    serialization: 'advanced',
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}
