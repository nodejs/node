// Flags: --expose-internals

'use strict';

require('../common');

const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('encoding_binding');

// Windows-1252 specific tests
{
  // Test Windows-1252 special characters in 128-159 range
  // These differ from Latin-1
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x80), false, false), '€');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x82), false, false), '‚');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x83), false, false), 'ƒ');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x9F), false, false), 'Ÿ');
}

{
  // Test Windows-1252 characters outside 128-159 range (same as Latin-1)
  const buf = Uint8Array.from([0xC1, 0xE9, 0xF3]);
  assert.strictEqual(binding.decodeWindows1252(buf, false, false), 'Áéó');
}

{
  // Empty input
  const buf = Uint8Array.from([]);
  assert.strictEqual(binding.decodeWindows1252(buf, false, false), '');
}

// Windows-1252 specific tests
{
  // Test Windows-1252 special characters in 128-159 range
  // These differ from Latin-1
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x80), false, false), '€');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x82), false, false), '‚');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x83), false, false), 'ƒ');
  assert.strictEqual(binding.decodeWindows1252(Uint8Array.of(0x9F), false, false), 'Ÿ');
}

{
  // Test Windows-1252 characters outside 128-159 range (same as Latin-1)
  const buf = Uint8Array.from([0xC1, 0xE9, 0xF3]);
  assert.strictEqual(binding.decodeWindows1252(buf, false, false), 'Áéó');
}

{
  // Empty input
  const buf = Uint8Array.from([]);
  assert.strictEqual(binding.decodeWindows1252(buf, false, false), '');
}
