'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.writeFile method.

const fs = require('fs');
const { open, writeFile } = fs.promises;
const path = require('path');
const { Readable } = require('stream');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWriteFile() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-file2.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  try {
    const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

    await fileHandle.writeFile(buffer);
    const readFileData = fs.readFileSync(filePathForHandle);
    assert.deepStrictEqual(buffer, readFileData);
  } finally {
    await fileHandle.close();
  }
}

// Signal aborted while writing file
async function doWriteAndCancel() {
  const filePathForHandle = path.resolve(tmpDir, 'dogs-running.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  try {
    const buffer = Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
    const controller = new AbortController();
    const { signal } = controller;
    process.nextTick(() => controller.abort());
    await assert.rejects(writeFile(fileHandle, buffer, { signal }), {
      name: 'AbortError'
    });
  } finally {
    await fileHandle.close();
  }
}

const dest = path.resolve(tmpDir, 'tmp.txt');
const otherDest = path.resolve(tmpDir, 'tmp-2.txt');
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

async function doWriteStream() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(stream);
    const expected = 'abc';
    const data = fs.readFileSync(dest, 'utf-8');
    assert.deepStrictEqual(data, expected);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteStreamWithCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  const fileHandle = await open(otherDest, 'w+');
  try {
    await assert.rejects(
      fileHandle.writeFile(stream, { signal }),
      { name: 'AbortError' }
    );
  } finally {
    await fileHandle.close();
  }
}

async function doWriteIterable() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(iterable);
    const data = fs.readFileSync(dest, 'utf-8');
    assert.deepStrictEqual(data, iterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteInvalidIterable() {
  const fileHandle = await open(dest, 'w+');
  try {
    await Promise.all(
      [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
        assert.rejects(
          fileHandle.writeFile(iterableWith(value)),
          { code: 'ERR_INVALID_ARG_TYPE' }
        )
      )
    );
  } finally {
    await fileHandle.close();
  }
}

async function doWriteIterableWithEncoding() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(stream2, 'latin1');
    const expected = 'ümlaut sechzig';
    const data = fs.readFileSync(dest, 'latin1');
    assert.deepStrictEqual(data, expected);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteBufferIterable() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(bufferIterable);
    const data = fs.readFileSync(dest, 'utf-8');
    assert.deepStrictEqual(data, bufferIterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteAsyncIterable() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(asyncIterable);
    const data = fs.readFileSync(dest, 'utf-8');
    assert.deepStrictEqual(data, asyncIterable.expected);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteInvalidValues() {
  const fileHandle = await open(dest, 'w+');
  try {
    await Promise.all(
      [42, 42n, {}, Symbol('42'), true, undefined, null, NaN].map((value) =>
        assert.rejects(
          fileHandle.writeFile(value),
          { code: 'ERR_INVALID_ARG_TYPE' }
        )
      )
    );
  } finally {
    await fileHandle.close();
  }
}

(async () => {
  await validateWriteFile();
  await doWriteAndCancel();
  await doWriteStream();
  await doWriteStreamWithCancel();
  await doWriteIterable();
  await doWriteInvalidIterable();
  await doWriteIterableWithEncoding();
  await doWriteBufferIterable();
  await doWriteAsyncIterable();
  await doWriteInvalidValues();
})().then(common.mustCall());
