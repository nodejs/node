'use strict';

const common = require('../common');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const { Readable } = require('stream');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function doWriteBuffer() {
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

async function doWriteString() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-string.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  try {
    const string = 'x~yz'.repeat(100);

    await fileHandle.writeFile(string);
    const readFileData = fs.readFileSync(filePathForHandle);
    const stringAsBuffer = Buffer.from(string, 'utf8');
    assert.deepStrictEqual(stringAsBuffer, readFileData);
  } finally {
    await fileHandle.close();
  }
}

async function doWriteBufferAndCancel() {
  const filePathForHandle = path.resolve(tmpDir, 'dogs-running.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  try {
    const buffer = Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
    const controller = new AbortController();
    const { signal } = controller;
    process.nextTick(() => controller.abort());
    await assert.rejects(fileHandle.writeFile(buffer, { signal }), {
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
  async*[Symbol.asyncIterator]() {
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

async function doWriteLargeIterable() {
  const fileHandle = await open(dest, 'w+');
  try {
    await fileHandle.writeFile(veryLargeIterable);
    const data = fs.readFileSync(dest, 'utf-8');
    assert.deepStrictEqual(data, veryLargeIterable.expected);
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

async function doWriteTypedArrays() {
  for (const Constructor of [Uint8Array, Uint16Array, Uint32Array]) {
    // Use a file size larger than `kReadFileMaxChunkSize`.
    const buffer = Buffer.from('012'.repeat(2 ** 14));
    const array = new Constructor(buffer.buffer);
    const fileHandle = await open(dest, 'w+');

    try {
      await fileHandle.writeFile(array);
      const data = fs.readFileSync(dest);
      assert.deepStrictEqual(data, buffer);
    } finally {
      await fileHandle.close();
    }
  }
}

async function doWriteFromCurrentPosition() {
  const fileHandle = await open(dest, 'w+');
  try {
    /* Write only five bytes, so that the position moves to five. */
    const buf = Buffer.from('Hello');
    const { bytesWritten } = await fileHandle.write(buf, 0, 5, null);
    assert.strictEqual(bytesWritten, 5);

    /* Write some more with writeFile(). */
    await fileHandle.writeFile('World');

    const data = fs.readFileSync(dest, 'utf-8');

    /* New content should be written at position five, instead of zero. */
    assert.strictEqual(data, 'HelloWorld');
  } finally {
    await fileHandle.close();
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
  await doWriteFromCurrentPosition();
})().then(common.mustCall());
