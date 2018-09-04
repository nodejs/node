'use strict';
const common = require('../common');
const assert = require('assert');

const { Readable, Writable } = require('stream');

const messageBuffer = Buffer.from('ABCDEFGH'); // Needs to be a multiple of 8
const toBeWritten = [...common.getArrayBufferViews(messageBuffer)];
const nthWrittenBuffer = (n) => Buffer.from(
  toBeWritten[n].buffer,
  toBeWritten[n].byteOffset,
  toBeWritten[n].byteLength
);

{
  // Simple Writable test.

  let n = 0;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert(Buffer.isBuffer(chunk));
      assert.deepStrictEqual(chunk, nthWrittenBuffer(n++));
      cb();
    }, toBeWritten.length)
  });

  toBeWritten.forEach((msg) => writable.write(msg));
  writable.end();
}

{
  // Writable test, pass in TypedArray in object mode.

  let n = 0;
  const writable = new Writable({
    objectMode: true,
    write: common.mustCall((chunk, encoding, cb) => {
      assert(!(chunk instanceof Buffer));
      assert(ArrayBuffer.isView(chunk));
      assert.strictEqual(chunk, toBeWritten[n]);
      assert.deepStrictEqual(chunk, toBeWritten[n]);
      assert.strictEqual(encoding, 'utf8');
      n++;
      cb();
    }, toBeWritten.length)
  });

  toBeWritten.forEach((msg) => writable.write(msg));
  writable.end();
}

{
  // Writable test, multiple writes carried out via writev.
  let callback;

  const writable = new Writable({
    write: common.mustCall((chunk, encoding, cb) => {
      assert(chunk instanceof Buffer);
      assert.strictEqual(encoding, 'buffer');
      assert.deepStrictEqual(chunk, nthWrittenBuffer(0));
      callback = cb;
    }),
    writev: common.mustCall((chunks, cb) => {
      const expectedWritevLength = toBeWritten.length - 1;
      assert.strictEqual(chunks.length, expectedWritevLength);
      for (let n = 0; n < expectedWritevLength; n++) {
        assert.deepStrictEqual(chunks[n].chunk, nthWrittenBuffer(n + 1));
      }
    })
  });

  toBeWritten.forEach((msg, index) => {
    if (index !== toBeWritten.length - 1) {
      writable.write(msg);
    } else {
      writable.end(msg);
    }
  });
  callback();
}

{
  // Simple Readable test.
  const readable = new Readable({
    read() {}
  });

  toBeWritten.forEach((wbuf) => readable.push(wbuf));
  readable.unshift(toBeWritten[0]);

  const buf = readable.read();
  assert(buf instanceof Buffer);
  const expectedWrittenBufferEntries = toBeWritten.map(
    (wbuf, index) => nthWrittenBuffer(index)
  ).reduce((acc, wbuf) => acc.concat(...wbuf), []);
  assert.deepStrictEqual([...buf], expectedWrittenBufferEntries);
}

{
  // Readable test, setEncoding.
  const readable = new Readable({
    read() {}
  });

  readable.setEncoding('utf8');

  toBeWritten.forEach((wbuf) => readable.push(wbuf));
  readable.unshift(toBeWritten[0]);

  const out = readable.read();
  assert.strictEqual(out, 'ABCDEFGH'.repeat(10));
}
