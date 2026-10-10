// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const { once } = require('node:events');
const { join } = require('node:path');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');

function immutable(value) {
  return Buffer.from(Uint8Array.from(Buffer.from(value)).buffer.transferToImmutable());
}

tmpdir.refresh();
let index = 0;
function makeDirectory() {
  const directory = tmpdir.resolve(`case-${index++}`);
  fs.mkdirSync(directory);
  return directory;
}

const readdirOptions = { recursive: true, withFileTypes: true };

for (const method of ['readdirSync', 'readdir', 'promises.readdir']) {
  test(`fs.${method} with a recursive Buffer path`, async () => {
    const directory = makeDirectory();
    fs.mkdirSync(join(directory, 'nested'));
    fs.writeFileSync(join(directory, 'nested', 'file'), 'contents');
    const path = immutable(directory);
    let entries;
    if (method === 'readdirSync') {
      entries = fs.readdirSync(path, readdirOptions);
    } else if (method === 'readdir') {
      entries = await new Promise((resolve) => {
        fs.readdir(path, readdirOptions, common.mustSucceed(resolve));
      });
    } else {
      entries = await fs.promises.readdir(path, readdirOptions);
    }
    assert.deepStrictEqual(entries.map((entry) => entry.name).sort(), ['file', 'nested']);
    const file = entries.find((entry) => entry.name === 'file');
    assert(file.isFile());
    assert.strictEqual(file.parentPath.toString(), join(directory, 'nested'));
    assert.strictEqual(path.toString(), directory);
  });
}

for (const method of ['rm', 'promises.rm']) {
  test(`fs.${method} with a recursive Buffer path`, async () => {
    const directory = makeDirectory();
    fs.writeFileSync(join(directory, 'file'), 'contents');
    const path = immutable(directory);
    if (method === 'rm') {
      await new Promise((resolve) => {
        fs.rm(path, { recursive: true }, common.mustSucceed(resolve));
      });
    } else {
      await fs.promises.rm(path, { recursive: true });
    }
    assert.strictEqual(fs.existsSync(directory), false);
    assert.strictEqual(path.toString(), directory);
  });
}

for (const sync of [false, true]) {
  test(`fs.Utf8Stream with ${sync ? 'merged synchronous' : 'asynchronous'} buffers`, async () => {
    const path = join(makeDirectory(), 'output');
    const stream = new fs.Utf8Stream({
      fd: fs.openSync(path, 'w'),
      contentMode: 'buffer',
      sync,
      // Buffer two chunks before the synchronous write so they must be merged.
      minLength: sync ? 8 : 0,
    });
    const closed = once(stream, 'close');
    try {
      stream.write(immutable('ABCD'));
      if (sync) stream.write(immutable('EFGH'));
      stream.end();
      await closed;
      assert.strictEqual(fs.readFileSync(path, 'utf8'), sync ? 'ABCDEFGH' : 'ABCD');
    } finally {
      stream.destroy();
      await closed;
    }
  });
}

test('fs.WriteStream retries a partial writev', async () => {
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
});
