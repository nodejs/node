// Flags: --experimental-vfs
'use strict';

// Test createReadStream / createWriteStream with an explicit `fd` option.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'hello world');

function readStream(stream) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    stream.on('data', (c) => chunks.push(c));
    stream.on('end', () => resolve(Buffer.concat(chunks).toString()));
    stream.on('error', reject);
  });
}

// Read using an existing fd; autoClose:false leaves fd open
{
  const fd = myVfs.openSync('/file.txt', 'r');
  const stream = myVfs.createReadStream('/file.txt', { fd, autoClose: false });
  let opened = false;
  stream.on('open', () => { opened = true; });
  readStream(stream).then(common.mustCall((s) => {
    assert.strictEqual(s, 'hello world');
    assert.strictEqual(opened, true);
    myVfs.closeSync(fd);
  }));
}

// WriteStream with explicit fd; autoClose:false leaves the fd open
(async () => {
  const fd = myVfs.openSync('/fd-write.txt', 'w');
  const stream = myVfs.createWriteStream('/fd-write.txt', { fd, autoClose: false });
  await new Promise((resolve) => stream.on('ready', resolve));
  await new Promise((resolve, reject) =>
    stream.end('via-fd', (err) => (err ? reject(err) : resolve())));
  myVfs.closeSync(fd);
  assert.strictEqual(myVfs.readFileSync('/fd-write.txt', 'utf8'), 'via-fd');
})().then(common.mustCall());

// WriteStream with explicit fd + start position
(async () => {
  myVfs.writeFileSync('/pad.txt', 'AAAAAAAAAA');
  const fd = myVfs.openSync('/pad.txt', 'r+');
  const ws = myVfs.createWriteStream('/pad.txt', { fd, start: 5, autoClose: false });
  await new Promise((resolve) => ws.on('ready', resolve));
  await new Promise((resolve, reject) =>
    ws.end('XX', (err) => (err ? reject(err) : resolve())));
  myVfs.closeSync(fd);
  // Position 5 → "AAAAAXXAAA"
  assert.strictEqual(myVfs.readFileSync('/pad.txt', 'utf8'), 'AAAAAXXAAA');
})().then(common.mustCall());
