'use strict';
require('../common');
const { StringDecoder } = require('string_decoder');
const util = require('util');
const assert = require('assert');

// Tests that, for random sequences of bytes, our StringDecoder gives the
// same result as a direction conversion using Buffer.toString().
// In particular, it checks that StringDecoder aligns with V8’s own output.

function rand(max) {
  return Math.floor(Math.random() * max);
}

function randBuf(maxLen) {
  const buf = Buffer.allocUnsafe(rand(maxLen));
  for (let i = 0; i < buf.length; i++)
    buf[i] = rand(256);
  return buf;
}

const encodings = [
  'utf16le', 'utf8', 'ascii', 'hex', 'base64', 'latin1', 'base64url',
];

function runSingleFuzzTest() {
  const enc = encodings[rand(encodings.length)];
  const sd = new StringDecoder(enc);
  const bufs = [];
  const strings = [];

  const N = rand(10);
  for (let i = 0; i < N; ++i) {
    const buf = randBuf(50);
    bufs.push(buf);
    strings.push(sd.write(buf));
  }
  strings.push(sd.end());

  assert.strictEqual(strings.join(''), Buffer.concat(bufs).toString(enc),
                     `Mismatch:\n${util.inspect(strings)}\n` +
                     util.inspect(bufs.map((buf) => buf.toString('hex'))) +
                     `\nfor encoding ${enc}`);
}

const start = Date.now();
while (Date.now() - start < 100)
  runSingleFuzzTest();
