'use strict';
// See https://github.com/nodejs/node/issues/5927

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdin.pipe(process.stdout);
  return;
}

const child = spawn(process.execPath, [__filename, 'child'], { stdio: 'pipe' });

const expectedBytes = 1024 * 1024;
let readBytes = 0;

child.stdin.end(Buffer.alloc(expectedBytes));

child.stdout.on('data', (chunk) => readBytes += chunk.length);
child.stdout.on('end', common.mustCall(() => {
  assert.strictEqual(readBytes, expectedBytes);
}));
