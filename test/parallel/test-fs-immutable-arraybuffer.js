// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const path = tmpdir.resolve('immutable-arraybuffer');
fs.writeFileSync(path, 'test');

function immutable() {
  return new Uint8Array(new ArrayBuffer(4).transferToImmutable());
}

async function checkRead(read, name = 'buffer') {
  const buffer = immutable();
  // Wrapped in an async function so that a synchronous throw and a rejected
  // promise are checked the same way.
  await assert.rejects(async () => read(buffer), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: `The ${name.includes('.') ? 'property' : 'argument'} '${name}' ` +
      'is backed by an immutable ArrayBuffer and cannot be written. ' +
      'Received Uint8Array(4) [ 0, 0, 0, 0 ]',
  });
  assert.deepStrictEqual([...buffer], [0, 0, 0, 0]);
}

const fdReads = [
  ['fs.readSync', (fd, buffer) => fs.readSync(fd, buffer, 0, 4, 0)],
  ['fs.readSync with options', (fd, buffer) => fs.readSync(fd, buffer, {})],
  ['fs.read', (fd, buffer) => new Promise((resolve, reject) => {
    fs.read(fd, buffer, 0, 4, 0, (err) => (err ? reject(err) : resolve()));
  })],
  ['fs.read with options', (fd, buffer) => new Promise((resolve, reject) => {
    fs.read(fd, { buffer }, (err) => (err ? reject(err) : resolve()));
  })],
  ['fs.readvSync', (fd, buffer) => fs.readvSync(fd, [buffer], 0), 'buffers[0]'],
  ['fs.readv', (fd, buffer) => new Promise((resolve, reject) => {
    fs.readv(fd, [buffer], 0, (err) => (err ? reject(err) : resolve()));
  }), 'buffers[0]'],
  ['fs.readvSync with a mutable buffer first',
   (fd, buffer) => fs.readvSync(fd, [Buffer.alloc(1), buffer], 0),
   'buffers[1]'],
];

for (const [name, read, argName] of fdReads) {
  test(name, async (t) => {
    const fd = fs.openSync(path, 'r');
    t.after(() => fs.closeSync(fd));
    await checkRead((buffer) => read(fd, buffer), argName);
    // The file position was not advanced by the rejected reads.
    assert.strictEqual(fs.readSync(fd, Buffer.alloc(4)), 4);
  });
}

const handleReads = [
  ['FileHandle.read', (handle, buffer) => handle.read(buffer, 0, 4, 0)],
  ['FileHandle.read with options', (handle, buffer) => handle.read({ buffer })],
  ['FileHandle.readv', (handle, buffer) => handle.readv([buffer], 0), 'buffers[0]'],
];

for (const [name, read, argName] of handleReads) {
  test(name, async (t) => {
    const handle = await fs.promises.open(path, 'r');
    t.after(() => handle.close());
    await checkRead((buffer) => read(handle, buffer), argName);
    const { bytesRead } = await handle.read(Buffer.alloc(4));
    assert.strictEqual(bytesRead, 4);
  });
}

const fileReads = [
  ['fs.readFileSync', (options) => fs.readFileSync(path, options)],
  ['fs.readFile', (options) => new Promise((resolve, reject) => {
    fs.readFile(path, options, (err) => (err ? reject(err) : resolve()));
  })],
  ['fs.promises.readFile', (options) => fs.promises.readFile(path, options)],
];

for (const factory of [false, true]) {
  const name = factory ? 'buffer factory' : 'buffer';
  const argName = factory ? 'options.buffer()' : 'options.buffer';
  const options = (buffer) => ({ buffer: factory ? () => buffer : buffer });

  for (const [method, read] of fileReads) {
    test(`${method} with ${name}`, () => {
      return checkRead((buffer) => read(options(buffer)), argName);
    });
  }

  test(`FileHandle.readFile with ${name}`, async (t) => {
    const handle = await fs.promises.open(path, 'r');
    t.after(() => handle.close());
    await checkRead((buffer) => handle.readFile(options(buffer)), argName);
  });
}
