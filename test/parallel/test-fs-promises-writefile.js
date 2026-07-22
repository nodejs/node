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

const dest = path.resolve(tmpDir, 'tmp.txt');
const otherDest = path.resolve(tmpDir, 'tmp-2.txt');
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

async function doWriteString() {
  const string = 'x~yz'.repeat(100);
  await fsPromises.writeFile(dest, string);
  const data = fs.readFileSync(dest);
  const stringAsBuffer = Buffer.from(string, 'utf8');
  assert.deepStrictEqual(stringAsBuffer, data);
}

async function doWriteBuffer() {
  await fsPromises.writeFile(dest, buffer);
  const data = fs.readFileSync(dest);
  assert.deepStrictEqual(data, buffer);
}

async function doWriteStream() {
  await fsPromises.writeFile(dest, stream);
  const expected = 'abc';
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, expected);
}

async function doWriteStreamWithCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.writeFile(otherDest, stream, { signal }),
    { name: 'AbortError' }
  );
}

async function doWriteIterable() {
  await fsPromises.writeFile(dest, iterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, iterable.expected);
}

async function doWriteInvalidIterable() {
  await Promise.all(
    [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
      assert.rejects(fsPromises.writeFile(dest, iterableWith(value)), {
        code: 'ERR_INVALID_ARG_TYPE',
      })
    )
  );
}

async function doWriteIterableWithEncoding() {
  await fsPromises.writeFile(dest, stream2, 'latin1');
  const expected = 'ümlaut sechzig';
  const data = fs.readFileSync(dest, 'latin1');
  assert.deepStrictEqual(data, expected);
}

async function doWriteBufferIterable() {
  await fsPromises.writeFile(dest, bufferIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, bufferIterable.expected);
}

async function doWriteAsyncIterable() {
  await fsPromises.writeFile(dest, asyncIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, asyncIterable.expected);
}

async function doWriteLargeIterable() {
  await fsPromises.writeFile(dest, veryLargeIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, veryLargeIterable.expected);
}

async function doWriteInvalidValues() {
  await Promise.all(
    [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
      assert.rejects(fsPromises.writeFile(dest, value), {
        code: 'ERR_INVALID_ARG_TYPE',
      })
    )
  );
}

async function doWriteBufferAndCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.writeFile(otherDest, buffer, { signal }),
    { name: 'AbortError' }
  );
}

async function doWriteTypedArrays() {
  for (const Constructor of [Uint8Array, Uint16Array, Uint32Array]) {
    // Use a file size larger than `kReadFileMaxChunkSize`.
    const buffer = Buffer.from('012'.repeat(2 ** 14));

    const array = new Constructor(buffer.buffer);
    await fsPromises.writeFile(dest, array);
    const data = await fsPromises.readFile(dest);
    assert.deepStrictEqual(data, buffer);
  }
}

(async () => {
  await doWriteBuffer();
  await doWriteBufferAndCancel();
  await doWriteString();
  await doWriteStream();
  await doWriteStreamWithCancel();
  await doWriteIterable();
  await doWriteInvalidIterable();
  await doWriteIterableWithEncoding();
  await doWriteBufferIterable();
  await doWriteAsyncIterable();
  await doWriteLargeIterable();
  await doWriteInvalidValues();
  await doWriteTypedArrays();
})().then(common.mustCall());
