'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-bigint-pos-' + process.pid);

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abcde');
myVfs.mount(mountPoint);

const filePath = path.join(mountPoint, 'file.txt');

// Test readSync with BigInt position
{
  const fd = fs.openSync(filePath, 'r');
  const buf = Buffer.alloc(1);
  const bytesRead = fs.readSync(fd, buf, 0, 1, 1n);
  assert.strictEqual(bytesRead, 1);
  assert.strictEqual(buf[0], 'b'.charCodeAt(0));
  fs.closeSync(fd);
}

// Test writeSync with BigInt position
{
  const fd = fs.openSync(filePath, 'r+');
  const buf = Buffer.from('X');
  const bytesWritten = fs.writeSync(fd, buf, 0, 1, 1n);
  assert.strictEqual(bytesWritten, 1);
  fs.closeSync(fd);

  // Verify write at position 1
  const content = fs.readFileSync(filePath, 'utf8');
  assert.strictEqual(content, 'aXcde');
}

myVfs.unmount();
