'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const join = require('path').join;
const { Readable } = require('stream');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  const filenameIterable = join(tmpdir.path, 'testIterable.txt');
  const iterable = {
    expected: 'abc',
    *[Symbol.iterator]() {
      yield 'a';
      yield 'b';
      yield 'c';
    }
  };

  fs.writeFile(filenameIterable, iterable, common.mustSucceed(() => {
    const data = fs.readFileSync(filenameIterable, 'utf-8');
    assert.strictEqual(iterable.expected, data);
  }));
}

{
  const filenameBufferIterable = join(tmpdir.path, 'testBufferIterable.txt');
  const bufferIterable = {
    expected: 'abc',
    *[Symbol.iterator]() {
      yield Buffer.from('a');
      yield Buffer.from('b');
      yield Buffer.from('c');
    }
  };

  fs.writeFile(
    filenameBufferIterable, bufferIterable, common.mustSucceed(() => {
      const data = fs.readFileSync(filenameBufferIterable, 'utf-8');
      assert.strictEqual(bufferIterable.expected, data);
    })
  );
}

{
  const filenameLargeBuffer = join(tmpdir.path, 'testLargeBuffer.txt');
  const largeBuffer = {
    expected: 'dogs running'.repeat(512 * 1024),
    *[Symbol.iterator]() {
      yield Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
    }
  };

  fs.writeFile(
    filenameLargeBuffer, largeBuffer, common.mustSucceed(() => {
      const data = fs.readFileSync(filenameLargeBuffer, 'utf-8');
      assert.strictEqual(largeBuffer.expected, data);
    })
  );
}

{
  const filenameBufferIterableWithEncoding =
    join(tmpdir.path, 'testBufferIterableWithEncoding.txt');
  const bufferIterableWithEncoding = {
    expected: 'ümlaut sechzig',
    *[Symbol.iterator]() {
      yield Buffer.from('ümlaut');
      yield Buffer.from(' ');
      yield Buffer.from('sechzig');
    }
  };

  fs.writeFile(
    filenameBufferIterableWithEncoding,
    bufferIterableWithEncoding, common.mustSucceed(() => {
      const data = fs.readFileSync(filenameBufferIterableWithEncoding, 'utf-8');
      assert.strictEqual(bufferIterableWithEncoding.expected, data);
    })
  );
}

{
  const filenameAsyncIterable = join(tmpdir.path, 'testAsyncIterable.txt');
  const asyncIterable = {
    expected: 'abc',
    *[Symbol.asyncIterator]() {
      yield 'a';
      yield 'b';
      yield 'c';
    }
  };

  fs.writeFile(filenameAsyncIterable, asyncIterable, common.mustSucceed(() => {
    const data = fs.readFileSync(filenameAsyncIterable, 'utf-8');
    assert.strictEqual(asyncIterable.expected, data);
  }));
}

{
  const filenameStream = join(tmpdir.path, 'testStream.txt');
  const stream = Readable.from(['a', 'b', 'c']);
  const expected = 'abc';

  fs.writeFile(filenameStream, stream, common.mustSucceed(() => {
    const data = fs.readFileSync(filenameStream, 'utf-8');
    assert.strictEqual(expected, data);
  }));
}

{
  const filenameStreamWithEncoding =
    join(tmpdir.path, 'testStreamWithEncoding.txt');
  const stream = Readable.from(['ümlaut', ' ', 'sechzig']);
  const expected = 'ümlaut sechzig';

  fs.writeFile(
    filenameStreamWithEncoding, stream, 'latin1', common.mustSucceed(() => {
      const data = fs.readFileSync(filenameStreamWithEncoding, 'latin1');
      assert.strictEqual(expected, data);
    })
  );
}

{
  const controller = new AbortController();
  const signal = controller.signal;
  const filenameIterableAbort = join(tmpdir.path, 'testIterableAbort.txt');
  const iterable = {
    expected: 'abc',
    *[Symbol.iterator]() {
      yield 'a';
      yield 'b';
      yield 'c';
    }
  };


  fs.writeFile(filenameIterableAbort,
               iterable, { signal }, common.mustCall((err) => {
                 assert.strictEqual(err.name, 'AbortError');
               }));

  controller.abort();
}

{
  const controller = new AbortController();
  const signal = controller.signal;
  const filenameAsyncIterableAbort =
    join(tmpdir.path, 'testAsyncIterableAbort.txt');
  const asyncIterable = {
    expected: 'abc',
    *[Symbol.asyncIterator]() {
      yield 'a';
      yield 'b';
      yield 'c';
    }
  };

  fs.writeFile(filenameAsyncIterableAbort,
               asyncIterable, { signal }, common.mustCall((err) => {
                 assert.strictEqual(err.name, 'AbortError');
               }));

  process.nextTick(() => controller.abort());
}
