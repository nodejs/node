'use strict';
require('../common');
const assert = require('assert');

// Test hex strings and bad hex strings
{
  const buf = Buffer.alloc(4);
  assert.strictEqual(buf.length, 4);
  assert.deepStrictEqual(buf, Buffer.from([0, 0, 0, 0]));
  assert.strictEqual(buf.write('abcdxx', 0, 'hex'), 2);
  assert.deepStrictEqual(buf, Buffer.from([0xab, 0xcd, 0x00, 0x00]));
  assert.strictEqual(buf.toString('hex'), 'abcd0000');
  assert.strictEqual(buf.write('abcdef01', 0, 'hex'), 4);
  assert.deepStrictEqual(buf, Buffer.from([0xab, 0xcd, 0xef, 0x01]));
  assert.strictEqual(buf.toString('hex'), 'abcdef01');

  const copy = Buffer.from(buf.toString('hex'), 'hex');
  assert.strictEqual(buf.toString('hex'), copy.toString('hex'));
}

{
  const buf = Buffer.alloc(5);
  assert.strictEqual(buf.write('abcdxx', 1, 'hex'), 2);
  assert.strictEqual(buf.toString('hex'), '00abcd0000');
}

{
  const buf = Buffer.alloc(4);
  assert.deepStrictEqual(buf, Buffer.from([0, 0, 0, 0]));
  assert.strictEqual(buf.write('xxabcd', 0, 'hex'), 0);
  assert.deepStrictEqual(buf, Buffer.from([0, 0, 0, 0]));
  assert.strictEqual(buf.write('xxab', 1, 'hex'), 0);
  assert.deepStrictEqual(buf, Buffer.from([0, 0, 0, 0]));
  assert.strictEqual(buf.write('cdxxab', 0, 'hex'), 1);
  assert.deepStrictEqual(buf, Buffer.from([0xcd, 0, 0, 0]));
}

{
  const buf = Buffer.alloc(256);
  for (let i = 0; i < 256; i++)
    buf[i] = i;

  const hex = buf.toString('hex');
  assert.deepStrictEqual(Buffer.from(hex, 'hex'), buf);

  const badHex = `${hex.slice(0, 256)}xx${hex.slice(256, 510)}`;
  assert.deepStrictEqual(Buffer.from(badHex, 'hex'), buf.slice(0, 128));
}
