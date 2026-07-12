'use strict';
require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  // Call .setEncoding() while there are bytes already in the buffer.
  const r = new Readable({ read() {} });

  r.push(Buffer.from('a'));
  r.push(Buffer.from('b'));

  r.setEncoding('utf8');
  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(() => {
    assert.deepStrictEqual(chunks, ['ab']);
  });
}

{
  // Call .setEncoding() while the buffer contains a complete,
  // but chunked character.
  const r = new Readable({ read() {} });

  r.push(Buffer.from([0xf0]));
  r.push(Buffer.from([0x9f]));
  r.push(Buffer.from([0x8e]));
  r.push(Buffer.from([0x89]));

  r.setEncoding('utf8');
  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(() => {
    assert.deepStrictEqual(chunks, ['🎉']);
  });
}

{
  // Call .setEncoding() while the buffer contains an incomplete character,
  // and finish the character later.
  const r = new Readable({ read() {} });

  r.push(Buffer.from([0xf0]));
  r.push(Buffer.from([0x9f]));

  r.setEncoding('utf8');

  r.push(Buffer.from([0x8e]));
  r.push(Buffer.from([0x89]));

  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(() => {
    assert.deepStrictEqual(chunks, ['🎉']);
  });
}

{
  // Call .setEncoding() before the stream starts flowing, after _read()
  // has buffered the first chunk of a split character.
  const chunks = [
    Buffer.from([0xf0, 0x9f]),
    Buffer.from([0x8e, 0x89]),
    null,
  ];
  const r = new Readable({
    read() {
      this.push(chunks.shift());
    },
  });

  r.read(0);
  assert.strictEqual(r.readableFlowing, null);
  assert.strictEqual(r.readableLength, 2);

  r.setEncoding('utf8');
  const received = [];
  r.on('data', (chunk) => received.push(chunk));

  process.nextTick(() => {
    assert.deepStrictEqual(received, ['🎉']);
  });
}

{
  // Call .setEncoding() after EOF while the buffer contains an
  // incomplete character.
  const r = new Readable({ read() {} });

  r.push(Buffer.from([0xf0]));
  r.push(Buffer.from([0x9f]));
  r.push(null);

  r.setEncoding('utf8');
  const chunks = [];
  r.on('data', (chunk) => chunks.push(chunk));

  process.nextTick(() => {
    assert.deepStrictEqual(chunks, ['�']);
  });
}
