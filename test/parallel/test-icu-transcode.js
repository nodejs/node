'use strict';

const common = require('../common');

if (!common.hasIntl)
  common.skip('missing Intl');

const buffer = require('buffer');
const assert = require('assert');
const orig = Buffer.from('tést €', 'utf8');

// Test Transcoding
const tests = {
  'latin1': [0x74, 0xe9, 0x73, 0x74, 0x20, 0x3f],
  'ascii': [0x74, 0x3f, 0x73, 0x74, 0x20, 0x3f],
  'ucs2': [0x74, 0x00, 0xe9, 0x00, 0x73,
           0x00, 0x74, 0x00, 0x20, 0x00,
           0xac, 0x20]
};

for (const test in tests) {
  const dest = buffer.transcode(orig, 'utf8', test);
  assert.strictEqual(dest.length, tests[test].length, `utf8->${test} length`);
  for (let n = 0; n < tests[test].length; n++)
    assert.strictEqual(dest[n], tests[test][n], `utf8->${test} char ${n}`);
}

{
  const dest = buffer.transcode(Buffer.from(tests.ucs2), 'ucs2', 'utf8');
  assert.strictEqual(dest.toString(), orig.toString());
}

{
  const utf8 = Buffer.from('€'.repeat(4000), 'utf8');
  const ucs2 = Buffer.from('€'.repeat(4000), 'ucs2');
  const utf8_to_ucs2 = buffer.transcode(utf8, 'utf8', 'ucs2');
  const ucs2_to_utf8 = buffer.transcode(ucs2, 'ucs2', 'utf8');
  assert.deepStrictEqual(utf8, ucs2_to_utf8);
  assert.deepStrictEqual(ucs2, utf8_to_ucs2);
  assert.strictEqual(ucs2_to_utf8.toString('utf8'),
                     utf8_to_ucs2.toString('ucs2'));
}

assert.throws(
  () => buffer.transcode(null, 'utf8', 'ascii'),
  /^TypeError: "source" argument must be a Buffer or Uint8Array$/
);

assert.throws(
  () => buffer.transcode(Buffer.from('a'), 'b', 'utf8'),
  /^Error: Unable to transcode Buffer \[U_ILLEGAL_ARGUMENT_ERROR\]/
);

assert.throws(
  () => buffer.transcode(Buffer.from('a'), 'uf8', 'b'),
  /^Error: Unable to transcode Buffer \[U_ILLEGAL_ARGUMENT_ERROR\]$/
);

assert.deepStrictEqual(
  buffer.transcode(Buffer.from('hi', 'ascii'), 'ascii', 'utf16le'),
  Buffer.from('hi', 'utf16le'));
assert.deepStrictEqual(
  buffer.transcode(Buffer.from('hi', 'latin1'), 'latin1', 'utf16le'),
  Buffer.from('hi', 'utf16le'));
assert.deepStrictEqual(
  buffer.transcode(Buffer.from('hä', 'latin1'), 'latin1', 'utf16le'),
  Buffer.from('hä', 'utf16le'));

// Test that Uint8Array arguments are okay.
{
  const uint8array = new Uint8Array([...Buffer.from('hä', 'latin1')]);
  assert.deepStrictEqual(
    buffer.transcode(uint8array, 'latin1', 'utf16le'),
    Buffer.from('hä', 'utf16le'));
}

{
  const dest = buffer.transcode(new Uint8Array(), 'utf8', 'latin1');
  assert.strictEqual(dest.length, 0);
}
