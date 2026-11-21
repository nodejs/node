'use strict';

// A flowing byte-mode Readable that pushes from _read() keeps the prefetched
// chunk off the buffer. Data put on the buffer by unshift(), push() or a
// nested read() while that chunk is pending must not be lost or reordered,
// and a setEncoding() from _read() must apply to the chunks pushed after it.

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

function source(chunks, read) {
  return new Readable({
    read: read ?? function() {
      this.push(chunks.shift() ?? null);
    },
  });
}

function collect(stream, ondata, expected) {
  const received = [];
  let first = true;
  stream.on('data', (chunk) => {
    if (first) {
      first = false;
      ondata(chunk, received);
    } else {
      received.push(chunk);
    }
  });
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(received).toString(), expected);
    assert.strictEqual(stream.readableLength, 0);
  }));
}

// unshift() from the 'data' listener.
{
  const stream = source([Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF')]);
  collect(stream, common.mustCall((chunk, received) => {
    received.push(chunk.subarray(0, 1));
    stream.unshift(chunk.subarray(1));
    assert.strictEqual(stream.readableLength, 3);
  }), 'ABCDEF');
}

// push() from the 'data' listener.
{
  const stream = source([Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF')]);
  collect(stream, common.mustCall((chunk, received) => {
    received.push(chunk);
    stream.push(Buffer.from('X'));
    assert.strictEqual(stream.readableLength, 3);
  }), 'ABCDXEF');
}

// unshift() from _read(), before and after push(). Every chunk must be
// delivered. The order is not checked.
{
  const chunks = [Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF')];
  let reads = 0;
  const stream = source(chunks, function() {
    reads++;
    if (reads === 2)
      this.unshift(Buffer.from('X'));
    this.push(chunks.shift() ?? null);
    if (reads === 3)
      this.unshift(Buffer.from('Y'));
  });
  const received = [];
  stream.on('data', (chunk) => received.push(...chunk.toString()));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(received.sort().join(''), 'ABCDEFXY');
    assert.strictEqual(stream.readableLength, 0);
  }));
}

// read(0) from _read() after push(). The nested _read() adds to the buffer.
{
  const chunks = [Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF'),
                  Buffer.from('GH')];
  let reads = 0;
  const stream = source(chunks, function() {
    reads++;
    this.push(chunks.shift() ?? null);
    if (reads === 2)
      this.read(0);
  });
  const received = [];
  stream.on('data', (chunk) => received.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(received).toString(), 'ABCDEFGH');
    assert.strictEqual(stream.readableLength, 0);
  }));
}

// read(0) from _read() between two push() calls.
{
  const chunks = [Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF'),
                  Buffer.from('GH'), Buffer.from('IJ')];
  let reads = 0;
  const stream = source(chunks, function() {
    reads++;
    this.push(chunks.shift() ?? null);
    if (reads === 2) {
      this.read(0);
      this.push(chunks.shift() ?? null);
    }
  });
  const received = [];
  stream.on('data', (chunk) => received.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.strictEqual(Buffer.concat(received).toString(), 'ABCDEFGHIJ');
    assert.strictEqual(stream.readableLength, 0);
  }));
}

// setEncoding() from _read(), before and after push(). The chunk delivered
// by the 'data' event in progress was read before the call and stays a
// Buffer. Every later chunk is a decoded string.
for (const before of [true, false]) {
  const chunks = [Buffer.from('AB'), Buffer.from('CD'), Buffer.from('EF')];
  let reads = 0;
  const stream = source(chunks, function() {
    reads++;
    if (reads === 2 && before)
      this.setEncoding('hex');
    this.push(chunks.shift() ?? null);
    if (reads === 2 && !before)
      this.setEncoding('hex');
  });
  const received = [];
  stream.on('data', (chunk) => received.push(chunk));
  stream.on('end', common.mustCall(() => {
    assert.deepStrictEqual(received[0], Buffer.from('AB'));
    assert.strictEqual(received.slice(1).join(''), '43444546');
    assert.strictEqual(stream.readableLength, 0);
  }));
}
