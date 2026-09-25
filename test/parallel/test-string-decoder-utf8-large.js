'use strict';
// The UTF-8 StringDecoder shares its byte->string conversion with
// Buffer#toString(): ASCII, Latin-1-representable and general inputs take
// different (SIMD) paths depending on content and size, and invalid input
// falls back to a replacing decoder. This test pins the decoder's output for
// inputs that cross those size thresholds, for chunkings that split multibyte
// sequences, and for invalid bytes embedded in otherwise large valid input.
require('../common');
const assert = require('assert');
const { StringDecoder } = require('string_decoder');

function decodeInChunks(buf, chunkSize) {
  const decoder = new StringDecoder('utf8');
  let out = '';
  for (let i = 0; i < buf.length; i += chunkSize) {
    out += decoder.write(buf.subarray(i, i + chunkSize));
  }
  return out + decoder.end();
}

function check(str, label) {
  const buf = Buffer.from(str, 'utf8');
  // Sanity: the expectation itself round-trips.
  assert.strictEqual(buf.toString('utf8'), str, `${label}: toString`);
  for (const chunkSize of [1, 2, 3, 4, 5, 7, 31, 32, 33, 255, 256, 257,
                           4095, 4096, 65536, buf.length]) {
    if (chunkSize > buf.length) continue;
    // Keep the test fast: byte-sized chunks only for the smaller inputs.
    if (buf.length > 100_000 && chunkSize < 4095) continue;
    assert.strictEqual(decodeInChunks(buf, chunkSize), str,
                       `${label}: chunkSize=${chunkSize}`);
  }
}

const sizes = [31, 32, 33, 255, 256, 257, 4096, 70000, (1 << 20) + 5];
for (const size of sizes) {
  check('a'.repeat(size), `ascii ${size}`);
  // Latin-1 range only (one-byte string in V8, two bytes each in UTF-8).
  check('é'.repeat(size), `latin1 ${size}`);
  // ASCII with a single Latin-1 character at the end / start.
  check('a'.repeat(size - 1) + 'ÿ', `ascii+latin1 tail ${size}`);
  check('Ä' + 'a'.repeat(size - 1), `latin1 head+ascii ${size}`);
  // BMP beyond Latin-1 (three-byte sequences).
  check('日'.repeat(size), `cjk ${size}`);
  // Mixed, including astral plane characters (surrogate pairs, 4 bytes).
  check(('abé日\u{1F600}').repeat(Math.ceil(size / 6)), `mixed ${size}`);
}

// Invalid bytes inside otherwise valid input of every size class must still be
// replaced with U+FFFD exactly as before, regardless of chunking.
for (const size of [8, 40, 300, 5000, (1 << 20) + 5]) {
  const valid = Buffer.from('a'.repeat(size));
  for (const bad of [[0xff], [0xc0, 0xaf], [0xe2, 0x28, 0xa1],
                     [0xed, 0xa0, 0x80] /* encoded surrogate */,
                     [0xf0, 0x9f, 0x98] /* truncated 4-byte */]) {
    const buf = Buffer.concat([valid, Buffer.from(bad), valid]);
    const expected = buf.toString('utf8');
    assert.ok(expected.includes('�'), `size=${size} bad=${bad}`);
    for (const chunkSize of [1, 3, 64, size, size + 1, buf.length]) {
      if (buf.length > 100_000 && chunkSize < size) continue;
      assert.strictEqual(decodeInChunks(buf, chunkSize), expected,
                         `invalid ${bad} in ${size}, chunkSize=${chunkSize}`);
    }
  }
}

// A lone continuation / lead byte split across the size classes at the very
// end is buffered by the decoder and flushed as U+FFFD by end().
{
  const decoder = new StringDecoder('utf8');
  const big = Buffer.concat([Buffer.from('a'.repeat(300)), Buffer.from([0xe2, 0x82])]);
  assert.strictEqual(decoder.write(big), 'a'.repeat(300));
  assert.strictEqual(decoder.end(), '�');
}
{
  const decoder = new StringDecoder('utf8');
  const big = Buffer.concat([Buffer.from('é'.repeat(300)), Buffer.from([0xe2, 0x82])]);
  assert.strictEqual(decoder.write(big), 'é'.repeat(300));
  assert.strictEqual(decoder.write(Buffer.from([0xac])), '€');
  assert.strictEqual(decoder.end(), '');
}
