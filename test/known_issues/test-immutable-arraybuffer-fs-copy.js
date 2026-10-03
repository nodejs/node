// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const { once } = require('node:events');
const { join } = require('node:path');
const { test } = require('node:test');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

// These APIs only read the supplied bytes. Internal copies must accept an
// immutable source just as they accept a mutable one.
function immutable(value) {
  return Buffer.from(Uint8Array.from(Buffer.from(value)).buffer.transferToImmutable());
}

const cases = new Map();
const options = { recursive: true, withFileTypes: true };

cases.set('fs.readdirSync with a recursive Buffer path', checkReaddirSync);

async function checkReaddirSync(directory) {
  fs.mkdirSync(join(directory, 'nested'));
  fs.writeFileSync(join(directory, 'nested', 'file'), 'contents');
  const path = immutable(directory);
  const entries = fs.readdirSync(path, options);
  assert.deepStrictEqual(entries.map((entry) => entry.name).sort(), ['file', 'nested']);
  const file = entries.find((entry) => entry.name === 'file');
  assert(file.isFile());
  assert.strictEqual(file.parentPath.toString(), join(directory, 'nested'));
}

cases.set('fs.readdir with a recursive Buffer path', checkReaddir);

async function checkReaddir(directory) {
  fs.mkdirSync(join(directory, 'nested'));
  fs.writeFileSync(join(directory, 'nested', 'file'), 'contents');
  const path = immutable(directory);
  const entries = await new Promise((resolve) => {
    fs.readdir(path, options, common.mustSucceed(resolve));
  });
  assert.deepStrictEqual(entries.map((entry) => entry.name).sort(), ['file', 'nested']);
  const file = entries.find((entry) => entry.name === 'file');
  assert(file.isFile());
  assert.strictEqual(file.parentPath.toString(), join(directory, 'nested'));
}

cases.set('fs.promises.readdir with a recursive Buffer path', checkReaddirPromise);

async function checkReaddirPromise(directory) {
  fs.mkdirSync(join(directory, 'nested'));
  fs.writeFileSync(join(directory, 'nested', 'file'), 'contents');
  const path = immutable(directory);
  const entries = await fs.promises.readdir(path, options);
  assert.deepStrictEqual(entries.map((entry) => entry.name).sort(), ['file', 'nested']);
  const file = entries.find((entry) => entry.name === 'file');
  assert(file.isFile());
  assert.strictEqual(file.parentPath.toString(), join(directory, 'nested'));
}

cases.set('fs.rm with a recursive Buffer path', checkRm);

async function checkRm(directory) {
  fs.writeFileSync(join(directory, 'file'), 'contents');
  const path = immutable(directory);
  await new Promise((resolve) => {
    fs.rm(path, { recursive: true }, common.mustSucceed(resolve));
  });
  assert.strictEqual(fs.existsSync(directory), false);
}

cases.set('fs.promises.rm with a recursive Buffer path', checkRmPromise);

async function checkRmPromise(directory) {
  fs.writeFileSync(join(directory, 'file'), 'contents');
  const path = immutable(directory);
  await fs.promises.rm(path, { recursive: true });
  assert.strictEqual(fs.existsSync(directory), false);
}

cases.set('fs.Utf8Stream with asynchronous buffers', checkUtf8Stream);

async function checkUtf8Stream(directory) {
  const path = join(directory, 'output');
  const stream = new fs.Utf8Stream({
    fd: fs.openSync(path, 'w'),
    contentMode: 'buffer',
    sync: false,
    minLength: 0,
  });
  const closed = once(stream, 'close');
  try {
    stream.write(immutable('ABCD'));
    stream.end();
    await closed;
    assert.strictEqual(fs.readFileSync(path, 'utf8'), 'ABCD');
  } finally {
    stream.destroy();
    await closed;
  }
}

cases.set('fs.Utf8Stream with merged synchronous buffers', checkUtf8StreamSync);

async function checkUtf8StreamSync(directory) {
  const path = join(directory, 'output');
  const stream = new fs.Utf8Stream({
    fd: fs.openSync(path, 'w'),
    contentMode: 'buffer',
    sync: true,
    // Buffer two chunks before the synchronous write so they must be merged.
    minLength: 8,
  });
  const closed = once(stream, 'close');
  try {
    stream.write(immutable('ABCD'));
    stream.write(immutable('EFGH'));
    stream.end();
    await closed;
    assert.strictEqual(fs.readFileSync(path, 'utf8'), 'ABCDEFGH');
  } finally {
    stream.destroy();
    await closed;
  }
}

cases.set('fs.WriteStream retries a partial writev', checkWriteStream);

async function checkWriteStream() {
  const output = [];
  let calls = 0;
  const stream = fs.createWriteStream(null, {
    // All descriptor operations are provided by the custom fs implementation.
    fd: 123,
    fs: {
      writev: common.mustCall((fd, buffers, position, callback) => {
        const bytes = buffers.flatMap((buffer) => [...buffer]);
        const written = calls++ === 0 ? 1 : bytes.length;
        output.push(...bytes.slice(0, written));
        process.nextTick(callback, null, written, buffers);
      }, 2),
      close: common.mustCall((fd, callback) => process.nextTick(callback, null)),
    },
  });
  const closed = once(stream, 'close');
  stream.cork();
  stream.write(immutable('ABCD'));
  stream.write(immutable('EFGH'));
  stream.end();
  await closed;
  assert.deepStrictEqual(output, [...Buffer.from('ABCDEFGH')]);
  assert.strictEqual(stream.bytesWritten, 8);
}

if (process.argv[2] === 'child') {
  // Some current failures escape an asynchronous callback or leave a promise
  // pending. Require completion even when the child otherwise exits normally.
  cases.get(process.argv[3])(process.argv[4]).then(common.mustCall());
} else {
  tmpdir.refresh();
  let index = 0;
  for (const name of cases.keys()) {
    test(name, () => {
      const directory = tmpdir.resolve(`case-${index++}`);
      fs.mkdirSync(directory);
      try {
        spawnSyncAndAssert(process.execPath, [
          ...process.execArgv, __filename, 'child', name, directory,
        ], {});
      } finally {
        fs.rmSync(directory, { recursive: true, force: true });
      }
    });
  }
}
