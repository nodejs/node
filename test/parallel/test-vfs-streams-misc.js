'use strict';

// Cover stream paths not exercised by other tests:
// - WriteStream basic write + close
// - createReadStream with start/end slicing
// - createReadStream with explicit fd
// - WriteStream with explicit fd
// - WriteStream with start position
// - error paths (open fails, EBADF on broken fd)

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const { pipeline } = require('stream/promises');
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

// Read with start/end slicing
readStream(myVfs.createReadStream('/file.txt', { start: 6, end: 10 }))
  .then(common.mustCall((s) => assert.strictEqual(s, 'world')));

// Read entire file
readStream(myVfs.createReadStream('/file.txt'))
  .then(common.mustCall((s) => assert.strictEqual(s, 'hello world')));

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

// Read of a nonexistent file emits 'error' (path not opened) — open is async
{
  const stream = myVfs.createReadStream('/missing.txt');
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// Write basic
(async () => {
  await pipeline(
    Readable.from([Buffer.from('hello'), Buffer.from(' world')]),
    myVfs.createWriteStream('/out.txt'),
  );
  assert.strictEqual(myVfs.readFileSync('/out.txt', 'utf8'), 'hello world');
})().then(common.mustCall());

// Write with start position writes from there onward
(async () => {
  myVfs.writeFileSync('/pad.txt', 'AAAAAAAAAA');
  await pipeline(
    Readable.from([Buffer.from('XX')]),
    myVfs.createWriteStream('/pad.txt', { start: 3, flags: 'r+' }),
  );
  const got = myVfs.readFileSync('/pad.txt', 'utf8');
  assert.strictEqual(got, 'AAAXXAAAAA');
})().then(common.mustCall());

// Write with string chunk + encoding
(async () => {
  const stream = myVfs.createWriteStream('/str.txt');
  await new Promise((resolve, reject) => {
    stream.write('hello', 'utf8', (err) => err ? reject(err) : resolve());
  });
  await new Promise((resolve) => stream.end(resolve));
  assert.strictEqual(myVfs.readFileSync('/str.txt', 'utf8'), 'hello');
})().then(common.mustCall());

// Write with explicit fd; autoClose:false leaves the fd open
(async () => {
  const fd = myVfs.openSync('/fd-write.txt', 'w');
  const stream = myVfs.createWriteStream('/fd-write.txt', { fd, autoClose: false });
  await new Promise((resolve) => stream.on('ready', resolve));
  await new Promise((resolve, reject) =>
    stream.end('via-fd', (err) => err ? reject(err) : resolve()));
  myVfs.closeSync(fd);
  assert.strictEqual(myVfs.readFileSync('/fd-write.txt', 'utf8'), 'via-fd');
})().then(common.mustCall());

// WriteStream with invalid path (no parent directory) emits error
{
  const stream = myVfs.createWriteStream('/non-existent-dir/file.txt');
  stream.on('error', common.mustCall((err) => {
    assert.ok(err);
  }));
}

// path getter
{
  const rs = myVfs.createReadStream('/file.txt');
  assert.strictEqual(rs.path, '/file.txt');
  rs.destroy();

  const ws = myVfs.createWriteStream('/p.txt');
  assert.strictEqual(ws.path, '/p.txt');
  ws.destroy();
}
