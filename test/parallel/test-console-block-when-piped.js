'use strict';

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const KB = 1024;
const MB = KB * KB;
const expected = KB * MB + MB; // 1GB + 1MB of newlines

// console.log must synchronously write to stdout if
// if the process is piped to. The expected behavior is that
// console.log will block the main thread if the consumer
// cannot keep up.
// See https://github.com/nodejs/node/issues/24992 for more
// details.

if (process.argv[2] === 'child') {
  const data = Buffer.alloc(KB).fill('x').toString();
  for (let i = 0; i < MB; i++)
    console.log(data);
  process.exit(0);
} else {
  const child = cp.spawn(process.execPath, [__filename, 'child'], {
    stdio: ['pipe', 'pipe', 'inherit']
  });
  let count = 0;
  child.stdout.on('data', (c) => count += c.length);
  child.stdout.on('end', common.mustCall(() => {
    assert.strictEqual(count, expected);
  }));
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
