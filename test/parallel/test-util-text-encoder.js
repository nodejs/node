'use strict';

require('../common');
const assert = require('assert');
const { TextEncoder, TextDecoder } = require('util');

const encoder = new TextEncoder();
const decoder = new TextDecoder();

assert.strictEqual(decoder.decode(encoder.encode('')), '');
assert.strictEqual(decoder.decode(encoder.encode('latin1')), 'latin1');
assert.strictEqual(decoder.decode(encoder.encode('Yağız')), 'Yağız');

// For loop is required to trigger the fast path for encodeInto
// Since v8 fast path is only triggered when v8 optimization starts.
for (let i = 0; i < 1e4; i++) {
  {
    const dest = new Uint8Array(10).fill(0);
    const encoded = encoder.encodeInto('Yağız', dest);
    assert.strictEqual(encoded.read, 5);
    assert.strictEqual(encoded.written, 7);
    assert.strictEqual(decoder.decode(dest), 'Yağız\x00\x00\x00');
  }

  {
    const dest = new Uint8Array(1024).fill(0);
    const encoded = encoder.encodeInto('latin', dest);
    assert.strictEqual(encoded.read, 5);
    assert.strictEqual(encoded.written, 5);
    assert.strictEqual(decoder.decode(dest.slice(0, 5)), 'latin');
  }

  {
    const input = 'latin';
    const dest = new Uint8Array(10).fill(0);
    const encoded = encoder.encodeInto(input, dest);
    assert.strictEqual(encoded.read, input.length);
    assert.strictEqual(encoded.written, input.length);
    assert.strictEqual(decoder.decode(dest.slice(0, 5)), 'latin');
  }

  {
    const input = 'latin';
    const dest = new Uint8Array(1).fill(0);
    const encoded = encoder.encodeInto(input, dest);
    assert.strictEqual(encoded.read, 1);
    assert.strictEqual(encoded.written, 1);
    assert.strictEqual(decoder.decode(dest), 'l');
  }
}
