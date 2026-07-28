'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.appendFile method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const { Readable } = require('stream');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

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
const veryLargeIterable = {
  expected: 'dogs running'.repeat(512 * 1024),
  *[Symbol.iterator]() {
    yield Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
  }
};

let counter = 0;

async function doAppendBuffer() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    const buffer = Buffer.from('a&Dp'.repeat(100), 'utf8');

    await fileHandle.appendFile(buffer);
    const appendedFileData = fs.readFileSync(filePath);
    assert.deepStrictEqual(appendedFileData, buffer);

  } finally {
    await fileHandle.close();
  }
}

async function doAppendString() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    const string = 'x~yz'.repeat(100);

    await fileHandle.appendFile(string);
    const stringAsBuffer = Buffer.from(string, 'utf8');
    const appendedFileData = fs.readFileSync(filePath);
    assert.deepStrictEqual(appendedFileData, stringAsBuffer);

  } finally {
    await fileHandle.close();
  }
}

async function doAppendBufferAndCancel() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    const buffer = Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
    const controller = new AbortController();
    const { signal } = controller;
    process.nextTick(() => controller.abort());
    await assert.rejects(fileHandle.appendFile(buffer, { signal }), {
      name: 'AbortError'
    });
  } finally {
    await fileHandle.close();
  }
}

async function doAppendStream() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(stream);
    const expected = 'abc';
    const data = fs.readFileSync(filePath, 'utf-8');
    assert.deepStrictEqual(data, expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendStreamWithCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await assert.rejects(
      fileHandle.appendFile(stream, { signal }),
      { name: 'AbortError' }
    );
  } finally {
    await fileHandle.close();
  }
}

async function doAppendIterable() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(iterable);
    const data = fs.readFileSync(filePath, 'utf-8');
    assert.deepStrictEqual(data, iterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendInvalidIterable() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await Promise.all(
      [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
        assert.rejects(
          fileHandle.appendFile(iterableWith(value)),
          { code: 'ERR_INVALID_ARG_TYPE' }
        )
      )
    );
  } finally {
    await fileHandle.close();
  }
}

async function doAppendIterableWithEncoding() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(stream2, 'latin1');
    const expected = 'ümlaut sechzig';
    const data = fs.readFileSync(filePath, 'latin1');
    assert.deepStrictEqual(data, expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendBufferIterable() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(bufferIterable);
    const data = fs.readFileSync(filePath, 'utf-8');
    assert.deepStrictEqual(data, bufferIterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendAsyncIterable() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(asyncIterable);
    const data = fs.readFileSync(filePath, 'utf-8');
    assert.deepStrictEqual(data, asyncIterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendLargeIterable() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await fileHandle.appendFile(veryLargeIterable);
    const data = fs.readFileSync(filePath, 'utf-8');
    assert.deepStrictEqual(data, veryLargeIterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doAppendInvalidValues() {
  const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
  const fileHandle = await open(filePath, 'a');
  try {
    await Promise.all(
      [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
        assert.rejects(
          fileHandle.appendFile(value),
          { code: 'ERR_INVALID_ARG_TYPE' }
        )
      )
    );
  } finally {
    await fileHandle.close();
  }
}

async function doAppendTypedArrays() {
  for (const Constructor of [Uint8Array, Uint16Array, Uint32Array]) {
    // Use a file size larger than `kReadFileMaxChunkSize`.
    const buffer = Buffer.from('012'.repeat(2 ** 14));
    const array = new Constructor(buffer.buffer);
    const filePath = path.resolve(tmpDir, `tmp-${counter++}.txt`);
    const fileHandle = await open(filePath, 'a');

    try {
      await fileHandle.writeFile(array);
      const data = fs.readFileSync(filePath);
      assert.deepStrictEqual(data, buffer);
    } finally {
      await fileHandle.close();
    }
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
