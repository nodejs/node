'use strict';

const common = require('../common');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;
const { Readable } = require('stream');

tmpdir.refresh();

const buffer = Buffer.from('abc'.repeat(1000));
const stream = Readable.from(['a', 'b', 'c']);
const stream2 = Readable.from(['ümlaut', ' ', 'sechzig']);
const iterable = {
  expected: 'abc',
  *[Symbol.iterator]() {
    yield 'a';
    yield 'b';
    yield 'c';
  }
};

const veryLargeIterable = {
  expected: 'dogs running'.repeat(512 * 1024),
  *[Symbol.iterator]() {
    yield Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
  }
};

function iterableWith(value) {
  return {
    *[Symbol.iterator]() {
      yield value;
    }
  };
}
const bufferIterable = {
  expected: 'abc',
  *[Symbol.iterator]() {
    yield Buffer.from('a');
    yield Buffer.from('b');
    yield Buffer.from('c');
  }
};
const asyncIterable = {
  expected: 'abc',
  async* [Symbol.asyncIterator]() {
    yield 'a';
    yield 'b';
    yield 'c';
  }
};

let counter = 0;

async function doAppendString() {
  const string = 'x~yz'.repeat(100);
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, string);
  const data = fs.readFileSync(dest);
  const stringAsBuffer = Buffer.from(string, 'utf8');
  assert.deepStrictEqual(stringAsBuffer, data);
}

async function doAppendBuffer() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, buffer);
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, buffer);
}

async function doAppendStream() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, stream);
  const expected = 'abc';
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, expected);
}

async function doAppendStreamWithCancel() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.appendFile(dest, stream, { signal }),
    { name: 'AbortError' }
  );
}

async function doAppendIterable() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, iterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, iterable.expected);
}

async function doAppendInvalidIterable() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await Promise.all(
    [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
      assert.rejects(fsPromises.appendFile(dest, iterableWith(value)), {
        code: 'ERR_INVALID_ARG_TYPE',
      })
    )
  );
}

async function doAppendIterableWithEncoding() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, stream2, 'latin1');
  const expected = 'ümlaut sechzig';
  const data = fs.readFileSync(dest, 'latin1');
  assert.deepStrictEqual(data, expected);
}

async function doAppendBufferIterable() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, bufferIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, bufferIterable.expected);
}

async function doAppendAsyncIterable() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, asyncIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, asyncIterable.expected);
}

async function doAppendLargeIterable() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await fsPromises.appendFile(dest, veryLargeIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, veryLargeIterable.expected);
}

async function doAppendInvalidValues() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  await Promise.all(
    [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
      assert.rejects(fsPromises.appendFile(dest, value), {
        code: 'ERR_INVALID_ARG_TYPE',
      })
    )
  );
}

async function doAppendBufferAndCancel() {
  const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.appendFile(dest, buffer, { signal }),
    { name: 'AbortError' }
  );
}

async function doAppendTypedArrays() {
  for (const Constructor of [Uint8Array, Uint16Array, Uint32Array]) {
    const dest = path.resolve(tmpDir, `tmp-${counter++}.txt`);

    // Use a file size larger than `kReadFileMaxChunkSize`.
    const buffer = Buffer.from('012'.repeat(2 ** 14));

    const array = new Constructor(buffer.buffer);
    await fsPromises.appendFile(dest, array);
    const data = await fsPromises.readFile(dest);
    assert.deepStrictEqual(data, buffer);
  }
}

(async () => {
  await doAppendBuffer();
  await doAppendBufferAndCancel();
  await doAppendString();
  await doAppendStream();
  await doAppendStreamWithCancel();
  await doAppendIterable();
  await doAppendInvalidIterable();
  await doAppendIterableWithEncoding();
  await doAppendBufferIterable();
  await doAppendAsyncIterable();
  await doAppendLargeIterable();
  await doAppendInvalidValues();
  await doAppendTypedArrays();
})().then(common.mustCall());
