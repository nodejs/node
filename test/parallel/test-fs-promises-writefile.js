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
const errorDest = path.resolve(tmpDir, 'tmp-error.txt');
const buffer = Buffer.from('abc'.repeat(1000));
const buffer2 = Buffer.from('xyz'.repeat(1000));
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
const streamLikeIterable = {
  expected: 'abc',
  pipe: common.mustNotCall('pipe should not be called for custom iterables'),
  on: common.mustNotCall('on should not be called without removeListener'),
  *[Symbol.iterator]() {
    yield 'a';
    yield 'b';
    yield 'c';
  }
};

const veryLargeBuffer = {
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

function createEarlyErrorStream(error) {
  const stream = new Readable({
    read() {}
  });
  process.nextTick(() => stream.destroy(error));
  return stream;
}

function createErroredStream(error) {
  const stream = new Readable({
    read() {}
  });
  stream.destroy(error);
  return stream;
}

function waitForNextTick() {
  return new Promise((resolve) => process.nextTick(resolve));
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

async function doWrite() {
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

async function doWriteStreamError() {
  const error = new Error('early writeFile stream error');
  const stream = createEarlyErrorStream(error);
  const uncaughtException = common.mustNotCall(
    'stream errors should reject writeFile()');

  process.once('uncaughtException', uncaughtException);
  try {
    await assert.rejects(
      fsPromises.writeFile(errorDest, stream),
      { message: error.message }
    );
    assert.strictEqual(stream.listenerCount('error'), 0);
  } finally {
    process.removeListener('uncaughtException', uncaughtException);
  }
}

async function doWriteAlreadyErroredStream() {
  const error = new Error('already errored writeFile stream');
  const stream = createErroredStream(error);
  const uncaughtException = common.mustNotCall(
    'already errored streams should reject writeFile()');

  process.once('uncaughtException', uncaughtException);
  try {
    await assert.rejects(
      fsPromises.writeFile(errorDest, stream),
      { message: error.message }
    );
    await waitForNextTick();
    assert.strictEqual(stream.listenerCount('error'), 0);
  } finally {
    process.removeListener('uncaughtException', uncaughtException);
  }
}

async function doWriteStreamOpenError() {
  const stream = Readable.from(['a']);

  await assert.rejects(
    fsPromises.writeFile(path.resolve(tmpDir, 'not-found', 'tmp.txt'), stream),
    { code: 'ENOENT' }
  );
  assert.strictEqual(stream.listenerCount('error'), 0);
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

async function doWriteStreamLikeIterable() {
  await fsPromises.writeFile(dest, streamLikeIterable);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, streamLikeIterable.expected);
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

async function doWriteAsyncLargeIterable() {
  await fsPromises.writeFile(dest, veryLargeBuffer);
  const data = fs.readFileSync(dest, 'utf-8');
  assert.deepStrictEqual(data, veryLargeBuffer.expected);
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

async function doWriteWithCancel() {
  const controller = new AbortController();
  const { signal } = controller;
  process.nextTick(() => controller.abort());
  await assert.rejects(
    fsPromises.writeFile(otherDest, buffer, { signal }),
    { name: 'AbortError' }
  );
}

async function doAppend() {
  await fsPromises.appendFile(dest, buffer2, { flag: null });
  const data = fs.readFileSync(dest);
  const buf = Buffer.concat([buffer, buffer2]);
  assert.deepStrictEqual(buf, data);
}

async function doRead() {
  const data = await fsPromises.readFile(dest);
  const buf = fs.readFileSync(dest);
  assert.deepStrictEqual(buf, data);
}

async function doReadWithEncoding() {
  const data = await fsPromises.readFile(dest, 'utf-8');
  const syncData = fs.readFileSync(dest, 'utf-8');
  assert.strictEqual(typeof data, 'string');
  assert.deepStrictEqual(data, syncData);
}

(async () => {
  await doWrite();
  await doWriteWithCancel();
  await doAppend();
  await doRead();
  await doReadWithEncoding();
  await doWriteStream();
  await doWriteStreamError();
  await doWriteAlreadyErroredStream();
  await doWriteStreamOpenError();
  await doWriteStreamWithCancel();
  await doWriteIterable();
  await doWriteStreamLikeIterable();
  await doWriteInvalidIterable();
  await doWriteIterableWithEncoding();
  await doWriteBufferIterable();
  await doWriteAsyncIterable();
  await doWriteAsyncLargeIterable();
  await doWriteInvalidValues();
})().then(common.mustCall());
