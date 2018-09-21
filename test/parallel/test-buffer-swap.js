'use strict';

require('../common');
const assert = require('assert');

// Test buffers small enough to use the JS implementation
{
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                           0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10]);

  assert.strictEqual(buf, buf.swap16());
  assert.deepStrictEqual(buf, Buffer.from([0x02, 0x01, 0x04, 0x03, 0x06, 0x05,
                                           0x08, 0x07, 0x0a, 0x09, 0x0c, 0x0b,
                                           0x0e, 0x0d, 0x10, 0x0f]));
  buf.swap16(); // restore

  assert.strictEqual(buf, buf.swap32());
  assert.deepStrictEqual(buf, Buffer.from([0x04, 0x03, 0x02, 0x01, 0x08, 0x07,
                                           0x06, 0x05, 0x0c, 0x0b, 0x0a, 0x09,
                                           0x10, 0x0f, 0x0e, 0x0d]));
  buf.swap32(); // restore

  assert.strictEqual(buf, buf.swap64());
  assert.deepStrictEqual(buf, Buffer.from([0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
                                           0x02, 0x01, 0x10, 0x0f, 0x0e, 0x0d,
                                           0x0c, 0x0b, 0x0a, 0x09]));
}

// Operates in-place
{
  const buf = Buffer.from([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7]);
  buf.slice(1, 5).swap32();
  assert.deepStrictEqual(buf, Buffer.from([0x1, 0x5, 0x4, 0x3, 0x2, 0x6, 0x7]));
  buf.slice(1, 5).swap16();
  assert.deepStrictEqual(buf, Buffer.from([0x1, 0x4, 0x5, 0x2, 0x3, 0x6, 0x7]));

  // Length assertions
  const re16 = /Buffer size must be a multiple of 16-bits/;
  const re32 = /Buffer size must be a multiple of 32-bits/;
  const re64 = /Buffer size must be a multiple of 64-bits/;

  assert.throws(() => Buffer.from(buf).swap16(), re16);
  assert.throws(() => Buffer.alloc(1025).swap16(), re16);
  assert.throws(() => Buffer.from(buf).swap32(), re32);
  assert.throws(() => buf.slice(1, 3).swap32(), re32);
  assert.throws(() => Buffer.alloc(1025).swap32(), re32);
  assert.throws(() => buf.slice(1, 3).swap64(), re64);
  assert.throws(() => Buffer.alloc(1025).swap64(), re64);
}

{
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                           0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10]);

  buf.slice(2, 18).swap64();

  assert.deepStrictEqual(buf, Buffer.from([0x01, 0x02, 0x0a, 0x09, 0x08, 0x07,
                                           0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
                                           0x10, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b,
                                           0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                           0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
                                           0x0f, 0x10]));
}

// Force use of native code (Buffer size above threshold limit for js impl)
{
  const bufData = new Uint32Array(256).fill(0x04030201);
  const buf = Buffer.from(bufData.buffer, bufData.byteOffset);
  const otherBufData = new Uint32Array(256).fill(0x03040102);
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  buf.swap16();
  assert.deepStrictEqual(buf, otherBuf);
}

{
  const bufData = new Uint32Array(256).fill(0x04030201);
  const buf = Buffer.from(bufData.buffer);
  const otherBufData = new Uint32Array(256).fill(0x01020304);
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  buf.swap32();
  assert.deepStrictEqual(buf, otherBuf);
}

{
  const bufData = new Uint8Array(256 * 8);
  const otherBufData = new Uint8Array(256 * 8);
  for (let i = 0; i < bufData.length; i++) {
    bufData[i] = i % 8;
    otherBufData[otherBufData.length - i - 1] = i % 8;
  }
  const buf = Buffer.from(bufData.buffer, bufData.byteOffset);
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  buf.swap64();
  assert.deepStrictEqual(buf, otherBuf);
}

// Test native code with buffers that are not memory-aligned
{
  const bufData = new Uint8Array(256 * 8);
  const otherBufData = new Uint8Array(256 * 8 - 2);
  for (let i = 0; i < bufData.length; i++) {
    bufData[i] = i % 2;
  }
  for (let i = 1; i < otherBufData.length; i++) {
    otherBufData[otherBufData.length - i] = (i + 1) % 2;
  }
  const buf = Buffer.from(bufData.buffer, bufData.byteOffset);
  // 0|1 0|1 0|1...
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  // 0|0 1|0 1|0...

  buf.slice(1, buf.length - 1).swap16();
  assert.deepStrictEqual(buf.slice(0, otherBuf.length), otherBuf);
}

{
  const bufData = new Uint8Array(256 * 8);
  const otherBufData = new Uint8Array(256 * 8 - 4);
  for (let i = 0; i < bufData.length; i++) {
    bufData[i] = i % 4;
  }
  for (let i = 1; i < otherBufData.length; i++) {
    otherBufData[otherBufData.length - i] = (i + 1) % 4;
  }
  const buf = Buffer.from(bufData.buffer, bufData.byteOffset);
  // 0|1 2 3 0|1 2 3...
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  // 0|0 3 2 1|0 3 2...

  buf.slice(1, buf.length - 3).swap32();
  assert.deepStrictEqual(buf.slice(0, otherBuf.length), otherBuf);
}

{
  const bufData = new Uint8Array(256 * 8);
  const otherBufData = new Uint8Array(256 * 8 - 8);
  for (let i = 0; i < bufData.length; i++) {
    bufData[i] = i % 8;
  }
  for (let i = 1; i < otherBufData.length; i++) {
    otherBufData[otherBufData.length - i] = (i + 1) % 8;
  }
  const buf = Buffer.from(bufData.buffer, bufData.byteOffset);
  // 0|1 2 3 4 5 6 7 0|1 2 3 4...
  const otherBuf = Buffer.from(otherBufData.buffer, otherBufData.byteOffset);
  // 0|0 7 6 5 4 3 2 1|0 7 6 5...

  buf.slice(1, buf.length - 7).swap64();
  assert.deepStrictEqual(buf.slice(0, otherBuf.length), otherBuf);
}
