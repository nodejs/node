'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-watchfile-bigint-' + process.pid);

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'hello');
myVfs.mount(mountPoint);

const filePath = path.join(mountPoint, 'file.txt');

// watchFile with bigint: true should return stats with bigint fields
fs.watchFile(
  filePath,
  { interval: 50, bigint: true },
  common.mustCall((curr, prev) => {
    assert.strictEqual(typeof curr.size, 'bigint');
    assert.strictEqual(typeof curr.mtimeMs, 'bigint');
    assert.strictEqual(typeof prev.size, 'bigint');
    fs.unwatchFile(filePath);
    myVfs.unmount();
  }),
);

// Trigger a change after a small delay
setTimeout(() => {
  fs.writeFileSync(filePath, 'hello world');
}, 150);
