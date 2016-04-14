'use strict';

require('../common');
const icu = require('icu');
const assert = require('assert');

const orig = Buffer.from('tést €', 'utf8');

assert.strictEqual(orig.length, 9);
assert.strictEqual(icu.utf8Length(orig), 6);

assert.strictEqual(icu.charAt(orig, 1), 'é');
assert.strictEqual(icu.codePointAt(orig, 1), 233);

assert.strictEqual(icu.charAt(orig, 6), '€');
assert.strictEqual(icu.charAt(orig, 7), '€');
assert.strictEqual(icu.charAt(orig, 8), '€');
assert.strictEqual(icu.charAt(orig, 9), undefined);
assert.strictEqual(icu.codePointAt(orig, 6), 8364);
assert.strictEqual(icu.codePointAt(orig, 7), 8364);
assert.strictEqual(icu.codePointAt(orig, 8), 8364);
assert.strictEqual(icu.codePointAt(orig, 9), undefined);


var n;
// Test Transcoding
const tests = {
  'latin1': [0x74, 0xe9, 0x73, 0x74, 0x20, 0x3f],
  'ascii': [0x74, 0x3f, 0x73, 0x74, 0x20, 0x3f],
  'ucs2': [0x74, 0x00, 0xe9, 0x00, 0x73,
           0x00, 0x74, 0x00, 0x20, 0x00,
           0xac, 0x20]
};

for (const test in tests) {
  const dest = icu.transcode(orig, 'utf8', test);
  assert.strictEqual(dest.length, tests[test].length);
  for (n = 0; n < tests[test].length; n++)
    assert.strictEqual(dest[n], tests[test][n]);
}

// Test UTF8 Slicing. The slicing needs to be aware of UTF-8 encoding
// boundaries so that the slice does not include UTF-8 partials.
const slice = [0xc3, 0xa9, 0x73, 0x74, 0x20, 0xe2, 0x82, 0xac];

{
  const sliced = icu.slice(orig, 'utf8', 1, 6);
  for (n = 0; n < slice.length; n++)
    assert.strictEqual(slice[n], sliced[n]);
}

{
  const sliced = icu.slice(orig, 'utf8', 1, 7);
  for (n = 0; n < slice.length; n++)
    assert.strictEqual(slice[n], sliced[n]);
}

{
  const sliced = icu.slice(orig, 'utf8', 1, 8);
  for (n = 0; n < slice.length; n++)
    assert.strictEqual(slice[n], sliced[n]);
}

// Test UCS2 Slicing. The slicing needs to be aware of UTF16 encoding
// boundaries so that the slice does not include UTF16 partials. Surrogate
// pairs are not considered.
{
  const sliced = icu.slice(Buffer.from('test', 'ucs2'), 'ucs2', 0, 2);
  assert.strictEqual(sliced.length, 4);
  assert.strictEqual(sliced[0], 0x74);
  assert.strictEqual(sliced[1], 0x00);
  assert.strictEqual(sliced[2], 0x65);
  assert.strictEqual(sliced[3], 0x00);
}

// Test Buffer Normalization
{
  const buf = icu.normalize(Buffer.from('\u1E9B\u0323', 'utf8'), 'nfc');
  assert.strictEqual(buf.length, 5);
  const res = [0xe1, 0xba, 0x9b, 0xcc, 0xa3];
  for (n = 0; n < buf.length; n++)
    assert.strictEqual(res[n], buf[n]);
}

{
  const buf = icu.normalize(Buffer.from('\u1E9B\u0323', 'utf8'), 'nfd');
  assert.strictEqual(buf.length, 6);
  const res = [0xc5, 0xbf, 0xcc, 0xa3, 0xcc, 0x87];
  for (n = 0; n < buf.length; n++)
    assert.strictEqual(res[n], buf[n]);
}

{
  const buf = icu.normalize(Buffer.from('\u1E9B\u0323', 'utf8'), 'nfkd');
  assert.strictEqual(buf.length, 5);
  const res = [0x73, 0xcc, 0xa3, 0xcc, 0x87];
  for (n = 0; n < buf.length; n++)
    assert.strictEqual(res[n], buf[n]);
}

{
  const buf = icu.normalize(Buffer.from('\u1E9B\u0323', 'utf8'), 'nfkc');
  assert.strictEqual(buf.length, 3);
  const res = [0xe1, 0xb9, 0xa9];
  for (n = 0; n < buf.length; n++)
    assert.strictEqual(res[n], buf[n]);
}

// Test column width
{
  assert.strictEqual(icu.getColumnWidth('a'), 1);
  assert.strictEqual(icu.getColumnWidth('丁'), 2);
  assert.strictEqual(icu.getColumnWidth('\ud83d\udc78\ud83c\udfff'), 2);
  assert.strictEqual(icu.getColumnWidth('👅'), 2);
  assert.strictEqual(icu.getColumnWidth('\n'), 0);
  assert.strictEqual(icu.getColumnWidth('\u200Ef\u200F'), 1);
  // Control chars and combining chars are zero
  assert.strictEqual(icu.getColumnWidth('\u200E\n\u220A\u20D2'), 1);

  // By default, ambiguous width codepoints are 1 column width
  assert.strictEqual(icu.getColumnWidth('■'), 1);
  assert.strictEqual(icu.getColumnWidth('■', true), 2);
}
