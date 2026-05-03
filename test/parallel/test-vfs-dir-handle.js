'use strict';

// Exercise the VirtualDir handle returned by opendirSync.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/d');
myVfs.writeFileSync('/d/a.txt', 'a');
myVfs.writeFileSync('/d/b.txt', 'b');
myVfs.mkdirSync('/d/sub');

// readSync iteration
{
  const dir = myVfs.opendirSync('/d');
  assert.strictEqual(dir.path, '/d');
  const names = [];
  let entry;
  while ((entry = dir.readSync()) !== null) {
    names.push(entry.name);
  }
  assert.deepStrictEqual(names.sort(), ['a.txt', 'b.txt', 'sub']);
  dir.closeSync();
  // closing again must throw
  assert.throws(() => dir.closeSync(), { code: 'ERR_DIR_CLOSED' });
  // reading after close throws
  assert.throws(() => dir.readSync(), { code: 'ERR_DIR_CLOSED' });
}

// for-await iteration
(async () => {
  const dir = myVfs.opendirSync('/d');
  const names = [];
  for await (const entry of dir) {
    names.push(entry.name);
  }
  assert.deepStrictEqual(names.sort(), ['a.txt', 'b.txt', 'sub']);
})().then(common.mustCall());

// async read with callback
(async () => {
  const dir = myVfs.opendirSync('/d');
  await new Promise((resolve, reject) => {
    dir.read((err, entry) => err ? reject(err) : resolve(entry));
  });
  await new Promise((resolve, reject) => {
    dir.close((err) => err ? reject(err) : resolve());
  });
})().then(common.mustCall());

// async read without callback returns a promise
(async () => {
  const dir = myVfs.opendirSync('/d');
  const entry = await dir.read();
  assert.ok(entry);
  await dir.close();
})().then(common.mustCall());

// using/explicit resource management
{
  const dir = myVfs.opendirSync('/d');
  dir[Symbol.dispose]();
  assert.throws(() => dir.readSync(), { code: 'ERR_DIR_CLOSED' });
}

// opendir (callback)
myVfs.opendir('/d', common.mustCall((err, dir) => {
  assert.ifError(err);
  assert.strictEqual(dir.path, '/d');
  dir.closeSync();
}));

// read() callback on a closed dir delivers ERR_DIR_CLOSED
{
  const dir = myVfs.opendirSync('/d');
  dir.closeSync();
  dir.read(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_DIR_CLOSED');
  }));
  // entries() iteration on a closed dir rejects with ERR_DIR_CLOSED
  (async () => {
    await assert.rejects(
      (async () => { for await (const _ of dir.entries()) {} })(), // eslint-disable-line no-unused-vars
      { code: 'ERR_DIR_CLOSED' });
  })().then(common.mustCall());
  // [Symbol.dispose] is a no-op on an already-closed dir (must not throw)
  dir[Symbol.dispose]();
}

// async dir.close() returns a promise when invoked without a callback
(async () => {
  const dir = myVfs.opendirSync('/d');
  await dir.close();
})().then(common.mustCall());

// opendirSync without options object
{
  const dir = myVfs.opendirSync('/d');
  dir.closeSync();
}

// opendir error path (missing directory)
myVfs.opendir('/missing-dir', common.mustCall((err) => {
  assert.ok(err);
}));
