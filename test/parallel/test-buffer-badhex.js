'use strict';
require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;

// Test hex strings and bad hex strings
{
  const buf1 = Buffer.alloc(4);
  assert.strictEqual(buf1.length, 4);
  assert.deepStrictEqual(buf1, new Buffer([0, 0, 0, 0]));
  assert.strictEqual(buf1.write('abcdxx', 0, 'hex'), 2);
  assert.deepStrictEqual(buf1, new Buffer([0xab, 0xcd, 0x00, 0x00]));
  assert.strictEqual(buf1.toString('hex'), 'abcd0000');
  assert.strictEqual(buf1.write('abcdef01', 0, 'hex'), 4);
  assert.deepStrictEqual(buf1, new Buffer([0xab, 0xcd, 0xef, 0x01]));
  assert.strictEqual(buf1.toString('hex'), 'abcdef01');

  const buf2 = Buffer.from(buf1.toString('hex'), 'hex');
  assert.strictEqual(buf1.toString('hex'), buf2.toString('hex'));

  const buf3 = Buffer.alloc(5);
  assert.strictEqual(buf3.write('abcdxx', 1, 'hex'), 2);
  assert.strictEqual(buf3.toString('hex'), '00abcd0000');

  const buf4 = Buffer.alloc(4);
  assert.deepStrictEqual(buf4, new Buffer([0, 0, 0, 0]));
  assert.strictEqual(buf4.write('xxabcd', 0, 'hex'), 0);
  assert.deepStrictEqual(buf4, new Buffer([0, 0, 0, 0]));
  assert.strictEqual(buf4.write('xxab', 1, 'hex'), 0);
  assert.deepStrictEqual(buf4, new Buffer([0, 0, 0, 0]));
  assert.strictEqual(buf4.write('cdxxab', 0, 'hex'), 1);
  assert.deepStrictEqual(buf4, new Buffer([0xcd, 0, 0, 0]));

  const buf5 = Buffer.alloc(256);
  for (let i = 0; i < 256; i++)
    buf5[i] = i;

  const hex = buf5.toString('hex');
  assert.deepStrictEqual(Buffer.from(hex, 'hex'), buf5);

  const badHex = hex.slice(0, 256) + 'xx' + hex.slice(256, 510);
  assert.deepStrictEqual(Buffer.from(badHex, 'hex'), buf5.slice(0, 128));
}
