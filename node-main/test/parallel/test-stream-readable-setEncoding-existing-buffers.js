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
