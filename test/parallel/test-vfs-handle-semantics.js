// Flags: --experimental-vfs
'use strict';

// A file handle on a mounted path must answer the same `node:fs` calls the
// way a descriptor on a real file does. These cases run against both the
// memory provider and the ZipProvider, and state the real-fs outcome as the
// expectation. Cases are independent so the runner reports each one.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const vfs = require('node:vfs');
const { test } = require('node:test');

const { O_WRONLY, O_RDONLY, O_CREAT } = fs.constants;

// Each provider is described by a function that mounts a fresh layer holding
// one file `f` with the given content and returns that file's path.
const providers = {
  memory(content) {
    const layer = vfs.create();
    layer.writeFileSync('/f', content);
    return path.join(layer.mount(), 'f');
  },
  zip(content) {
    const entry = zlib.ZipEntry.createSync('f', Buffer.from(content));
    const chunks = [];
    for (const chunk of zlib.createZipArchiveSync([entry])) chunks.push(chunk);
    const provider = new vfs.ZipProvider(new zlib.ZipBuffer(Buffer.concat(chunks)));
    return path.join(vfs.create(provider).mount(), 'f');
  },
};

for (const { 0: name, 1: fileWith } of Object.entries(providers)) {
  test(`${name}: reading a write-only handle fails with EBADF`, () => {
    const fd = fs.openSync(fileWith('x'), 'w');
    try {
      assert.throws(() => fs.readSync(fd, Buffer.alloc(4), 0, 4, 0), { code: 'EBADF' });
    } finally {
      fs.closeSync(fd);
    }
  });

  test(`${name}: writing a read-only handle fails with EBADF`, () => {
    const fd = fs.openSync(fileWith('x'), 'r');
    try {
      assert.throws(() => fs.writeSync(fd, Buffer.from('y')), { code: 'EBADF' });
    } finally {
      fs.closeSync(fd);
    }
  });

  test(`${name}: extending a file with ftruncate zero-fills the new region`, () => {
    const file = fileWith('hello world');
    const fd = fs.openSync(file, 'r+');
    fs.ftruncateSync(fd, 5);
    fs.ftruncateSync(fd, 11);
    fs.closeSync(fd);
    // Shrinking then growing must not resurrect the bytes that were cut off.
    assert.strictEqual(fs.readFileSync(file, 'latin1'), 'hello\0\0\0\0\0\0');
  });

  test(`${name}: readSync accepts a BigInt position`, () => {
    const fd = fs.openSync(fileWith('hello'), 'r');
    try {
      const buf = Buffer.alloc(4);
      const n = fs.readSync(fd, buf, 0, 4, 1n);
      assert.strictEqual(buf.toString('utf8', 0, n), 'ello');
    } finally {
      fs.closeSync(fd);
    }
  });

  test(`${name}: numeric O_WRONLY does not truncate`, () => {
    const file = fileWith('hello');
    const fd = fs.openSync(file, O_WRONLY);
    fs.writeSync(fd, Buffer.from('J'), 0, 1, 0);
    fs.closeSync(fd);
    // Only O_TRUNC truncates.
    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'Jello');
  });

  test(`${name}: numeric O_RDONLY | O_CREAT opens an existing file readable and intact`, () => {
    const file = fileWith('hello');
    const fd = fs.openSync(file, O_RDONLY | O_CREAT);
    try {
      const buf = Buffer.alloc(5);
      const n = fs.readSync(fd, buf, 0, 5, 0);
      assert.strictEqual(buf.toString('utf8', 0, n), 'hello');
    } finally {
      fs.closeSync(fd);
    }
    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'hello');
  });

  test(`${name}: an "a+" handle reads from the start of the file`, () => {
    const fd = fs.openSync(fileWith('abc'), 'a+');
    try {
      const buf = Buffer.alloc(3);
      // O_APPEND only moves writes to the end; the read offset starts at 0.
      const n = fs.readSync(fd, buf, 0, 3, null);
      assert.strictEqual(buf.toString('utf8', 0, n), 'abc');
    } finally {
      fs.closeSync(fd);
    }
  });

  test(`${name}: writeFileSync with flag "r+" overwrites in place without truncating`, () => {
    const file = fileWith('hello world');
    fs.writeFileSync(file, 'HEY', { flag: 'r+' });
    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'HEYlo world');
  });
}
