// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');

// Reading into an immutable backing store must not change its bytes.
tmpdir.refresh();
const path = tmpdir.resolve('immutable-arraybuffer');
fs.writeFileSync(path, 'test');

async function checkRead(read) {
  const buffer = new Uint8Array(new ArrayBuffer(4).transferToImmutable());
  try {
    await read(buffer);
  } catch (err) {
    if (!(err instanceof TypeError)) throw err;
  }
  assert.deepStrictEqual([...buffer], [0, 0, 0, 0]);
}

const fdReads = [
  ['fs.readSync', (fd, buffer) => fs.readSync(fd, buffer, 0, 4, 0)],
  ['fs.read', (fd, buffer) => new Promise((resolve, reject) => {
    fs.read(fd, buffer, 0, 4, 0, (err) => (err ? reject(err) : resolve()));
  })],
  ['fs.readvSync', (fd, buffer) => fs.readvSync(fd, [buffer], 0)],
  ['fs.readv', (fd, buffer) => new Promise((resolve, reject) => {
    fs.readv(fd, [buffer], 0, (err) => (err ? reject(err) : resolve()));
  })],
];

for (const [name, read] of fdReads) {
  test(name, async (t) => {
    const fd = fs.openSync(path, 'r');
    t.after(() => fs.closeSync(fd));
    await checkRead((buffer) => read(fd, buffer));
  });
}

for (const method of ['read', 'readv']) {
  test(`FileHandle.${method}`, async (t) => {
    const handle = await fs.promises.open(path, 'r');
    t.after(() => handle.close());
    await checkRead((buffer) => (method === 'read' ?
      handle.read(buffer, 0, 4, 0) : handle.readv([buffer], 0)));
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
  const options = (buffer) => ({ buffer: factory ? () => buffer : buffer });

  for (const [method, read] of fileReads) {
    test(`${method} with ${name}`, () => {
      return checkRead((buffer) => read(options(buffer)));
    });
  }

  test(`FileHandle.readFile with ${name}`, async (t) => {
    const handle = await fs.promises.open(path, 'r');
    t.after(() => handle.close());
    await checkRead((buffer) => handle.readFile(options(buffer)));
  });
}
