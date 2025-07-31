'use strict';

require('../common');

// Test verifies that the toBase64, fromBase64, toHex, and fromHex methods
// of Uint8Array are available and work as expected.

const { strictEqual } = require('assert');
const { Buffer } = require('buffer');

const buf = Buffer.from('hello');

const checkBase64 = buf.toString('base64');
const checkHex = buf.toString('hex');

const enc = new TextEncoder();

strictEqual(enc.encode('hello').toBase64(), checkBase64);
strictEqual(enc.encode('hello').toHex(), checkHex);

const dec = new TextDecoder();
strictEqual(dec.decode(Uint8Array.fromBase64(checkBase64)), 'hello');
strictEqual(dec.decode(Uint8Array.fromHex(checkHex)), 'hello');
