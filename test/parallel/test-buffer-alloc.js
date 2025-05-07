'use strict';
const common = require('../common');

const assert = require('assert');
const vm = require('vm');

const {
  SlowBuffer,
  kMaxLength,
} = require('buffer');

// Verify the maximum Uint8Array size. There is no concrete limit by spec. The
// internal limits should be updated if this fails.
assert.throws(
  () => new Uint8Array(kMaxLength + 1),
  { message: `Invalid typed array length: ${kMaxLength + 1}` },
);

const b = Buffer.allocUnsafe(1024);
assert.strictEqual(b.length, 1024);

b[0] = -1;
assert.strictEqual(b[0], 255);

for (let i = 0; i < 1024; i++) {
  b[i] = i % 256;
}

for (let i = 0; i < 1024; i++) {
  assert.strictEqual(i % 256, b[i]);
}

const c = Buffer.allocUnsafe(512);
assert.strictEqual(c.length, 512);

const d = Buffer.from([]);
assert.strictEqual(d.length, 0);

// Test offset properties
{
  const b = Buffer.alloc(128);
  assert.strictEqual(b.length, 128);
  assert.strictEqual(b.byteOffset, 0);
  assert.strictEqual(b.offset, 0);
}

// Test creating a Buffer from a Uint8Array
{
  const ui8 = new Uint8Array(4).fill(42);
  const e = Buffer.from(ui8);
  for (const [index, value] of e.entries()) {
    assert.strictEqual(value, ui8[index]);
  }
}
// Test creating a Buffer from a Uint8Array (old constructor)
{
  const ui8 = new Uint8Array(4).fill(42);
  const e = Buffer(ui8);
  for (const [key, value] of e.entries()) {
    assert.strictEqual(value, ui8[key]);
  }
}

// Test creating a Buffer from a Uint32Array
// Note: it is implicitly interpreted as Array of integers modulo 256
{
  const ui32 = new Uint32Array(4).fill(42);
  const e = Buffer.from(ui32);
  for (const [index, value] of e.entries()) {
    assert.strictEqual(value, ui32[index]);
  }
}
// Test creating a Buffer from a Uint32Array (old constructor)
// Note: it is implicitly interpreted as Array of integers modulo 256
{
  const ui32 = new Uint32Array(4).fill(42);
  const e = Buffer(ui32);
  for (const [key, value] of e.entries()) {
    assert.strictEqual(value, ui32[key]);
  }
}

// Test invalid encoding for Buffer.toString
assert.throws(() => b.toString('invalid'),
              /Unknown encoding: invalid/);
// Invalid encoding for Buffer.write
assert.throws(() => b.write('test string', 0, 5, 'invalid'),
              /Unknown encoding: invalid/);
// Unsupported arguments for Buffer.write
assert.throws(() => b.write('test', 'utf8', 0),
              { code: 'ERR_INVALID_ARG_TYPE' });

// Try to create 0-length buffers. Should not throw.
Buffer.from('');
Buffer.from('', 'ascii');
Buffer.from('', 'latin1');
Buffer.alloc(0);
Buffer.allocUnsafe(0);
new Buffer('');
new Buffer('', 'ascii');
new Buffer('', 'latin1');
new Buffer('', 'binary');
Buffer(0);

const outOfRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError'
};

// Try to write a 0-length string beyond the end of b
assert.throws(() => b.write('', 2048), outOfRangeError);

// Throw when writing to negative offset
assert.throws(() => b.write('a', -1), outOfRangeError);

// Throw when writing past bounds from the pool
assert.throws(() => b.write('a', 2048), outOfRangeError);

// Throw when writing to negative offset
assert.throws(() => b.write('a', -1), outOfRangeError);

// Try to copy 0 bytes worth of data into an empty buffer
b.copy(Buffer.alloc(0), 0, 0, 0);

// Try to copy 0 bytes past the end of the target buffer
b.copy(Buffer.alloc(0), 1, 1, 1);
b.copy(Buffer.alloc(1), 1, 1, 1);

// Try to copy 0 bytes from past the end of the source buffer
b.copy(Buffer.alloc(1), 0, 1024, 1024);

// Testing for smart defaults and ability to pass string values as offset
{
  const writeTest = Buffer.from('abcdes');
  writeTest.write('n', 'ascii');
  assert.throws(
    () => writeTest.write('o', '1', 'ascii'),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
  writeTest.write('o', 1, 'ascii');
  writeTest.write('d', 2, 'ascii');
  writeTest.write('e', 3, 'ascii');
  writeTest.write('j', 4, 'ascii');
  assert.strictEqual(writeTest.toString(), 'nodejs');
}

// Offset points to the end of the buffer and does not throw.
// (see https://github.com/nodejs/node/issues/8127).
Buffer.alloc(1).write('', 1, 0);

// ASCII slice test
{
  const asciiString = 'hello world';

  for (let i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }
  const asciiSlice = b.toString('ascii', 0, asciiString.length);
  assert.strictEqual(asciiString, asciiSlice);
}

{
  const asciiString = 'hello world';
  const offset = 100;

  assert.strictEqual(asciiString.length, b.write(asciiString, offset, 'ascii'));
  const asciiSlice = b.toString('ascii', offset, offset + asciiString.length);
  assert.strictEqual(asciiString, asciiSlice);
}

{
  const asciiString = 'hello world';
  const offset = 100;

  const sliceA = b.slice(offset, offset + asciiString.length);
  const sliceB = b.slice(offset, offset + asciiString.length);
  for (let i = 0; i < asciiString.length; i++) {
    assert.strictEqual(sliceA[i], sliceB[i]);
  }
}

// UTF-8 slice test
{
  const utf8String = '¡hέlló wôrld!';
  const offset = 100;

  b.write(utf8String, 0, Buffer.byteLength(utf8String), 'utf8');
  let utf8Slice = b.toString('utf8', 0, Buffer.byteLength(utf8String));
  assert.strictEqual(utf8String, utf8Slice);

  assert.strictEqual(Buffer.byteLength(utf8String),
                     b.write(utf8String, offset, 'utf8'));
  utf8Slice = b.toString('utf8', offset,
                         offset + Buffer.byteLength(utf8String));
  assert.strictEqual(utf8String, utf8Slice);

  const sliceA = b.slice(offset, offset + Buffer.byteLength(utf8String));
  const sliceB = b.slice(offset, offset + Buffer.byteLength(utf8String));
  for (let i = 0; i < Buffer.byteLength(utf8String); i++) {
    assert.strictEqual(sliceA[i], sliceB[i]);
  }
}

{
  const slice = b.slice(100, 150);
  assert.strictEqual(slice.length, 50);
  for (let i = 0; i < 50; i++) {
    assert.strictEqual(b[100 + i], slice[i]);
  }
}

{
  // Make sure only top level parent propagates from allocPool
  const b = Buffer.allocUnsafe(5);
  const c = b.slice(0, 4);
  const d = c.slice(0, 2);
  assert.strictEqual(b.parent, c.parent);
  assert.strictEqual(b.parent, d.parent);
}

{
  // Also from a non-pooled instance
  const b = Buffer.allocUnsafeSlow(5);
  const c = b.slice(0, 4);
  const d = c.slice(0, 2);
  assert.strictEqual(c.parent, d.parent);
}

{
  // Bug regression test
  const testValue = '\u00F6\u65E5\u672C\u8A9E'; // ö日本語
  const buffer = Buffer.allocUnsafe(32);
  const size = buffer.write(testValue, 0, 'utf8');
  const slice = buffer.toString('utf8', 0, size);
  assert.strictEqual(slice, testValue);
}

{
  // Test triple  slice
  const a = Buffer.allocUnsafe(8);
  for (let i = 0; i < 8; i++) a[i] = i;
  const b = a.slice(4, 8);
  assert.strictEqual(b[0], 4);
  assert.strictEqual(b[1], 5);
  assert.strictEqual(b[2], 6);
  assert.strictEqual(b[3], 7);
  const c = b.slice(2, 4);
  assert.strictEqual(c[0], 6);
  assert.strictEqual(c[1], 7);
}

{
  const d = Buffer.from([23, 42, 255]);
  assert.strictEqual(d.length, 3);
  assert.strictEqual(d[0], 23);
  assert.strictEqual(d[1], 42);
  assert.strictEqual(d[2], 255);
  assert.deepStrictEqual(d, Buffer.from(d));
}

{
  // Test for proper UTF-8 Encoding
  const e = Buffer.from('über');
  assert.deepStrictEqual(e, Buffer.from([195, 188, 98, 101, 114]));
}

{
  // Test for proper ascii Encoding, length should be 4
  const f = Buffer.from('über', 'ascii');
  assert.deepStrictEqual(f, Buffer.from([252, 98, 101, 114]));
}

['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach((encoding) => {
  {
    // Test for proper UTF16LE encoding, length should be 8
    const f = Buffer.from('über', encoding);
    assert.deepStrictEqual(f, Buffer.from([252, 0, 98, 0, 101, 0, 114, 0]));
  }

  {
    // Length should be 12
    const f = Buffer.from('привет', encoding);
    assert.deepStrictEqual(
      f, Buffer.from([63, 4, 64, 4, 56, 4, 50, 4, 53, 4, 66, 4])
    );
    assert.strictEqual(f.toString(encoding), 'привет');
  }

  {
    const f = Buffer.from([0, 0, 0, 0, 0]);
    assert.strictEqual(f.length, 5);
    const size = f.write('あいうえお', encoding);
    assert.strictEqual(size, 4);
    assert.deepStrictEqual(f, Buffer.from([0x42, 0x30, 0x44, 0x30, 0x00]));
  }
});

{
  const f = Buffer.from('\uD83D\uDC4D', 'utf-16le'); // THUMBS UP SIGN (U+1F44D)
  assert.strictEqual(f.length, 4);
  assert.deepStrictEqual(f, Buffer.from('3DD84DDC', 'hex'));
}

// Test construction from arrayish object
{
  const arrayIsh = { 0: 0, 1: 1, 2: 2, 3: 3, length: 4 };
  let g = Buffer.from(arrayIsh);
  assert.deepStrictEqual(g, Buffer.from([0, 1, 2, 3]));
  const strArrayIsh = { 0: '0', 1: '1', 2: '2', 3: '3', length: 4 };
  g = Buffer.from(strArrayIsh);
  assert.deepStrictEqual(g, Buffer.from([0, 1, 2, 3]));
}

//
// Test toString('base64')
//
assert.strictEqual((Buffer.from('Man')).toString('base64'), 'TWFu');
assert.strictEqual((Buffer.from('Woman')).toString('base64'), 'V29tYW4=');

//
// Test toString('base64url')
//
assert.strictEqual((Buffer.from('Man')).toString('base64url'), 'TWFu');
assert.strictEqual((Buffer.from('Woman')).toString('base64url'), 'V29tYW4');

{
  // Test that regular and URL-safe base64 both work both ways
  const expected = [0xff, 0xff, 0xbe, 0xff, 0xef, 0xbf, 0xfb, 0xef, 0xff];
  assert.deepStrictEqual(Buffer.from('//++/++/++//', 'base64'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('__--_--_--__', 'base64'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('//++/++/++//', 'base64url'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('__--_--_--__', 'base64url'),
                         Buffer.from(expected));
}

const base64flavors = ['base64', 'base64url'];

{
  // Test that regular and URL-safe base64 both work both ways with padding
  const expected = [0xff, 0xff, 0xbe, 0xff, 0xef, 0xbf, 0xfb, 0xef, 0xff, 0xfb];
  assert.deepStrictEqual(Buffer.from('//++/++/++//+w==', 'base64'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('//++/++/++//+w==', 'base64'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('//++/++/++//+w==', 'base64url'),
                         Buffer.from(expected));
  assert.deepStrictEqual(Buffer.from('//++/++/++//+w==', 'base64url'),
                         Buffer.from(expected));
}

{
  // big example
  const quote = 'Man is distinguished, not only by his reason, but by this ' +
                'singular passion from other animals, which is a lust ' +
                'of the mind, that by a perseverance of delight in the ' +
                'continued and indefatigable generation of knowledge, ' +
                'exceeds the short vehemence of any carnal pleasure.';
  const expected = 'TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb' +
                   '24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlci' +
                   'BhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQ' +
                   'gYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu' +
                   'dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ' +
                   'GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm' +
                   '5hbCBwbGVhc3VyZS4=';
  assert.strictEqual(Buffer.from(quote).toString('base64'), expected);
  assert.strictEqual(
    Buffer.from(quote).toString('base64url'),
    expected.replaceAll('+', '-').replaceAll('/', '_').replaceAll('=', '')
  );

  base64flavors.forEach((encoding) => {
    let b = Buffer.allocUnsafe(1024);
    let bytesWritten = b.write(expected, 0, encoding);
    assert.strictEqual(quote.length, bytesWritten);
    assert.strictEqual(quote, b.toString('ascii', 0, quote.length));

    // Check that the base64 decoder ignores whitespace
    const expectedWhite = `${expected.slice(0, 60)} \n` +
                          `${expected.slice(60, 120)} \n` +
                          `${expected.slice(120, 180)} \n` +
                          `${expected.slice(180, 240)} \n` +
                          `${expected.slice(240, 300)}\n` +
                          `${expected.slice(300, 360)}\n`;
    b = Buffer.allocUnsafe(1024);
    bytesWritten = b.write(expectedWhite, 0, encoding);
    assert.strictEqual(quote.length, bytesWritten);
    assert.strictEqual(quote, b.toString('ascii', 0, quote.length));

    // Check that the base64 decoder on the constructor works
    // even in the presence of whitespace.
    b = Buffer.from(expectedWhite, encoding);
    assert.strictEqual(quote.length, b.length);
    assert.strictEqual(quote, b.toString('ascii', 0, quote.length));

    // Check that the base64 decoder ignores illegal chars
    const expectedIllegal = expected.slice(0, 60) + ' \x80' +
                            expected.slice(60, 120) + ' \xff' +
                            expected.slice(120, 180) + ' \x00' +
                            expected.slice(180, 240) + ' \x98' +
                            expected.slice(240, 300) + '\x03' +
                            expected.slice(300, 360);
    b = Buffer.from(expectedIllegal, encoding);
    assert.strictEqual(quote.length, b.length);
    assert.strictEqual(quote, b.toString('ascii', 0, quote.length));
  });
}

base64flavors.forEach((encoding) => {
  assert.strictEqual(Buffer.from('', encoding).toString(), '');
  assert.strictEqual(Buffer.from('K', encoding).toString(), '');

  // multiple-of-4 with padding
  assert.strictEqual(Buffer.from('Kg==', encoding).toString(), '*');
  assert.strictEqual(Buffer.from('Kio=', encoding).toString(), '*'.repeat(2));
  assert.strictEqual(Buffer.from('Kioq', encoding).toString(), '*'.repeat(3));
  assert.strictEqual(
    Buffer.from('KioqKg==', encoding).toString(), '*'.repeat(4));
  assert.strictEqual(
    Buffer.from('KioqKio=', encoding).toString(), '*'.repeat(5));
  assert.strictEqual(
    Buffer.from('KioqKioq', encoding).toString(), '*'.repeat(6));
  assert.strictEqual(Buffer.from('KioqKioqKg==', encoding).toString(),
                     '*'.repeat(7));
  assert.strictEqual(Buffer.from('KioqKioqKio=', encoding).toString(),
                     '*'.repeat(8));
  assert.strictEqual(Buffer.from('KioqKioqKioq', encoding).toString(),
                     '*'.repeat(9));
  assert.strictEqual(Buffer.from('KioqKioqKioqKg==', encoding).toString(),
                     '*'.repeat(10));
  assert.strictEqual(Buffer.from('KioqKioqKioqKio=', encoding).toString(),
                     '*'.repeat(11));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioq', encoding).toString(),
                     '*'.repeat(12));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKg==', encoding).toString(),
                     '*'.repeat(13));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKio=', encoding).toString(),
                     '*'.repeat(14));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKioq', encoding).toString(),
                     '*'.repeat(15));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKg==', encoding).toString(),
    '*'.repeat(16));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKio=', encoding).toString(),
    '*'.repeat(17));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKioq', encoding).toString(),
    '*'.repeat(18));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKioqKioqKg==',
                                 encoding).toString(),
                     '*'.repeat(19));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKioqKioqKio=',
                                 encoding).toString(),
                     '*'.repeat(20));

  // No padding, not a multiple of 4
  assert.strictEqual(Buffer.from('Kg', encoding).toString(), '*');
  assert.strictEqual(Buffer.from('Kio', encoding).toString(), '*'.repeat(2));
  assert.strictEqual(Buffer.from('KioqKg', encoding).toString(), '*'.repeat(4));
  assert.strictEqual(
    Buffer.from('KioqKio', encoding).toString(), '*'.repeat(5));
  assert.strictEqual(Buffer.from('KioqKioqKg', encoding).toString(),
                     '*'.repeat(7));
  assert.strictEqual(Buffer.from('KioqKioqKio', encoding).toString(),
                     '*'.repeat(8));
  assert.strictEqual(Buffer.from('KioqKioqKioqKg', encoding).toString(),
                     '*'.repeat(10));
  assert.strictEqual(Buffer.from('KioqKioqKioqKio', encoding).toString(),
                     '*'.repeat(11));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKg', encoding).toString(),
                     '*'.repeat(13));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKio', encoding).toString(),
                     '*'.repeat(14));
  assert.strictEqual(Buffer.from('KioqKioqKioqKioqKioqKg', encoding).toString(),
                     '*'.repeat(16));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKio', encoding).toString(),
    '*'.repeat(17));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKioqKg', encoding).toString(),
    '*'.repeat(19));
  assert.strictEqual(
    Buffer.from('KioqKioqKioqKioqKioqKioqKio', encoding).toString(),
    '*'.repeat(20));
});

// Handle padding graciously, multiple-of-4 or not
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw==', 'base64').length,
  32
);
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9-VgdGPFJDxUBFR5_rMFsghgxADiw==', 'base64url')
    .length,
  32
);
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw=', 'base64').length,
  32
);
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9-VgdGPFJDxUBFR5_rMFsghgxADiw=', 'base64url')
    .length,
  32
);
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw', 'base64').length,
  32
);
assert.strictEqual(
  Buffer.from('72INjkR5fchcxk9-VgdGPFJDxUBFR5_rMFsghgxADiw', 'base64url')
    .length,
  32
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg==', 'base64').length,
  31
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg==', 'base64url')
    .length,
  31
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg=', 'base64').length,
  31
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg=', 'base64url')
    .length,
  31
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg', 'base64').length,
  31
);
assert.strictEqual(
  Buffer.from('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg', 'base64url').length,
  31
);

{
// This string encodes single '.' character in UTF-16
  const dot = Buffer.from('//4uAA==', 'base64');
  assert.strictEqual(dot[0], 0xff);
  assert.strictEqual(dot[1], 0xfe);
  assert.strictEqual(dot[2], 0x2e);
  assert.strictEqual(dot[3], 0x00);
  assert.strictEqual(dot.toString('base64'), '//4uAA==');
}

{
// This string encodes single '.' character in UTF-16
  const dot = Buffer.from('//4uAA', 'base64url');
  assert.strictEqual(dot[0], 0xff);
  assert.strictEqual(dot[1], 0xfe);
  assert.strictEqual(dot[2], 0x2e);
  assert.strictEqual(dot[3], 0x00);
  assert.strictEqual(dot.toString('base64url'), '__4uAA');
}

{
  // Writing base64 at a position > 0 should not mangle the result.
  //
  // https://github.com/joyent/node/issues/402
  const segments = ['TWFkbmVzcz8h', 'IFRoaXM=', 'IGlz', 'IG5vZGUuanMh'];
  const b = Buffer.allocUnsafe(64);
  let pos = 0;

  for (let i = 0; i < segments.length; ++i) {
    pos += b.write(segments[i], pos, 'base64');
  }
  assert.strictEqual(b.toString('latin1', 0, pos),
                     'Madness?! This is node.js!');
}

{
  // Writing base64url at a position > 0 should not mangle the result.
  //
  // https://github.com/joyent/node/issues/402
  const segments = ['TWFkbmVzcz8h', 'IFRoaXM', 'IGlz', 'IG5vZGUuanMh'];
  const b = Buffer.allocUnsafe(64);
  let pos = 0;

  for (let i = 0; i < segments.length; ++i) {
    pos += b.write(segments[i], pos, 'base64url');
  }
  assert.strictEqual(b.toString('latin1', 0, pos),
                     'Madness?! This is node.js!');
}

// Regression test for https://github.com/nodejs/node/issues/3496.
assert.strictEqual(Buffer.from('=bad'.repeat(1e4), 'base64').length, 0);

// Regression test for https://github.com/nodejs/node/issues/11987.
assert.deepStrictEqual(Buffer.from('w0  ', 'base64'),
                       Buffer.from('w0', 'base64'));

// Regression test for https://github.com/nodejs/node/issues/13657.
assert.deepStrictEqual(Buffer.from(' YWJvcnVtLg', 'base64'),
                       Buffer.from('YWJvcnVtLg', 'base64'));

{
  // Creating buffers larger than pool size.
  const l = Buffer.poolSize + 5;
  const s = 'h'.repeat(l);
  const b = Buffer.from(s);

  for (let i = 0; i < l; i++) {
    assert.strictEqual(b[i], 'h'.charCodeAt(0));
  }

  const sb = b.toString();
  assert.strictEqual(sb.length, s.length);
  assert.strictEqual(sb, s);
}

{
  // test hex toString
  const hexb = Buffer.allocUnsafe(256);
  for (let i = 0; i < 256; i++) {
    hexb[i] = i;
  }
  const hexStr = hexb.toString('hex');
  assert.strictEqual(hexStr,
                     '000102030405060708090a0b0c0d0e0f' +
                     '101112131415161718191a1b1c1d1e1f' +
                     '202122232425262728292a2b2c2d2e2f' +
                     '303132333435363738393a3b3c3d3e3f' +
                     '404142434445464748494a4b4c4d4e4f' +
                     '505152535455565758595a5b5c5d5e5f' +
                     '606162636465666768696a6b6c6d6e6f' +
                     '707172737475767778797a7b7c7d7e7f' +
                     '808182838485868788898a8b8c8d8e8f' +
                     '909192939495969798999a9b9c9d9e9f' +
                     'a0a1a2a3a4a5a6a7a8a9aaabacadaeaf' +
                     'b0b1b2b3b4b5b6b7b8b9babbbcbdbebf' +
                     'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf' +
                     'd0d1d2d3d4d5d6d7d8d9dadbdcdddedf' +
                     'e0e1e2e3e4e5e6e7e8e9eaebecedeeef' +
                     'f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff');

  const hexb2 = Buffer.from(hexStr, 'hex');
  for (let i = 0; i < 256; i++) {
    assert.strictEqual(hexb2[i], hexb[i]);
  }
}

// Test single hex character is discarded.
assert.strictEqual(Buffer.from('A', 'hex').length, 0);

// Test that if a trailing character is discarded, rest of string is processed.
assert.deepStrictEqual(Buffer.from('Abx', 'hex'), Buffer.from('Ab', 'hex'));

// Test single base64 char encodes as 0.
assert.strictEqual(Buffer.from('A', 'base64').length, 0);


{
  // Test an invalid slice end.
  const b = Buffer.from([1, 2, 3, 4, 5]);
  const b2 = b.toString('hex', 1, 10000);
  const b3 = b.toString('hex', 1, 5);
  const b4 = b.toString('hex', 1);
  assert.strictEqual(b2, b3);
  assert.strictEqual(b2, b4);
}

function buildBuffer(data) {
  if (Array.isArray(data)) {
    const buffer = Buffer.allocUnsafe(data.length);
    data.forEach((v, k) => buffer[k] = v);
    return buffer;
  }
  return null;
}

const x = buildBuffer([0x81, 0xa3, 0x66, 0x6f, 0x6f, 0xa3, 0x62, 0x61, 0x72]);

assert.strictEqual(x.inspect(), '<Buffer 81 a3 66 6f 6f a3 62 61 72>');

{
  const z = x.slice(4);
  assert.strictEqual(z.length, 5);
  assert.strictEqual(z[0], 0x6f);
  assert.strictEqual(z[1], 0xa3);
  assert.strictEqual(z[2], 0x62);
  assert.strictEqual(z[3], 0x61);
  assert.strictEqual(z[4], 0x72);
}

{
  const z = x.slice(0);
  assert.strictEqual(z.length, x.length);
}

{
  const z = x.slice(0, 4);
  assert.strictEqual(z.length, 4);
  assert.strictEqual(z[0], 0x81);
  assert.strictEqual(z[1], 0xa3);
}

{
  const z = x.slice(0, 9);
  assert.strictEqual(z.length, 9);
}

{
  const z = x.slice(1, 4);
  assert.strictEqual(z.length, 3);
  assert.strictEqual(z[0], 0xa3);
}

{
  const z = x.slice(2, 4);
  assert.strictEqual(z.length, 2);
  assert.strictEqual(z[0], 0x66);
  assert.strictEqual(z[1], 0x6f);
}

['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach((encoding) => {
  const b = Buffer.allocUnsafe(10);
  b.write('あいうえお', encoding);
  assert.strictEqual(b.toString(encoding), 'あいうえお');
});

['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach((encoding) => {
  const b = Buffer.allocUnsafe(11);
  b.write('あいうえお', 1, encoding);
  assert.strictEqual(b.toString(encoding, 1), 'あいうえお');
});

{
  // latin1 encoding should write only one byte per character.
  const b = Buffer.from([0xde, 0xad, 0xbe, 0xef]);
  let s = String.fromCharCode(0xffff);
  b.write(s, 0, 'latin1');
  assert.strictEqual(b[0], 0xff);
  assert.strictEqual(b[1], 0xad);
  assert.strictEqual(b[2], 0xbe);
  assert.strictEqual(b[3], 0xef);
  s = String.fromCharCode(0xaaee);
  b.write(s, 0, 'latin1');
  assert.strictEqual(b[0], 0xee);
  assert.strictEqual(b[1], 0xad);
  assert.strictEqual(b[2], 0xbe);
  assert.strictEqual(b[3], 0xef);
}

{
  // Binary encoding should write only one byte per character.
  const b = Buffer.from([0xde, 0xad, 0xbe, 0xef]);
  let s = String.fromCharCode(0xffff);
  b.write(s, 0, 'latin1');
  assert.strictEqual(b[0], 0xff);
  assert.strictEqual(b[1], 0xad);
  assert.strictEqual(b[2], 0xbe);
  assert.strictEqual(b[3], 0xef);
  s = String.fromCharCode(0xaaee);
  b.write(s, 0, 'latin1');
  assert.strictEqual(b[0], 0xee);
  assert.strictEqual(b[1], 0xad);
  assert.strictEqual(b[2], 0xbe);
  assert.strictEqual(b[3], 0xef);
}

{
  // https://github.com/nodejs/node-v0.x-archive/pull/1210
  // Test UTF-8 string includes null character
  let buf = Buffer.from('\0');
  assert.strictEqual(buf.length, 1);
  buf = Buffer.from('\0\0');
  assert.strictEqual(buf.length, 2);
}

{
  const buf = Buffer.allocUnsafe(2);
  assert.strictEqual(buf.write(''), 0); // 0bytes
  assert.strictEqual(buf.write('\0'), 1); // 1byte (v8 adds null terminator)
  assert.strictEqual(buf.write('a\0'), 2); // 1byte * 2
  assert.strictEqual(buf.write('あ'), 0); // 3bytes
  assert.strictEqual(buf.write('\0あ'), 1); // 1byte + 3bytes
  assert.strictEqual(buf.write('\0\0あ'), 2); // 1byte * 2 + 3bytes
}

{
  const buf = Buffer.allocUnsafe(10);
  assert.strictEqual(buf.write('あいう'), 9); // 3bytes * 3 (v8 adds null term.)
  assert.strictEqual(buf.write('あいう\0'), 10); // 3bytes * 3 + 1byte
}

{
  // https://github.com/nodejs/node-v0.x-archive/issues/243
  // Test write() with maxLength
  const buf = Buffer.allocUnsafe(4);
  buf.fill(0xFF);
  assert.strictEqual(buf.write('abcd', 1, 2, 'utf8'), 2);
  assert.strictEqual(buf[0], 0xFF);
  assert.strictEqual(buf[1], 0x61);
  assert.strictEqual(buf[2], 0x62);
  assert.strictEqual(buf[3], 0xFF);

  buf.fill(0xFF);
  assert.strictEqual(buf.write('abcd', 1, 4), 3);
  assert.strictEqual(buf[0], 0xFF);
  assert.strictEqual(buf[1], 0x61);
  assert.strictEqual(buf[2], 0x62);
  assert.strictEqual(buf[3], 0x63);

  buf.fill(0xFF);
  assert.strictEqual(buf.write('abcd', 1, 2, 'utf8'), 2);
  assert.strictEqual(buf[0], 0xFF);
  assert.strictEqual(buf[1], 0x61);
  assert.strictEqual(buf[2], 0x62);
  assert.strictEqual(buf[3], 0xFF);

  buf.fill(0xFF);
  assert.strictEqual(buf.write('abcdef', 1, 2, 'hex'), 2);
  assert.strictEqual(buf[0], 0xFF);
  assert.strictEqual(buf[1], 0xAB);
  assert.strictEqual(buf[2], 0xCD);
  assert.strictEqual(buf[3], 0xFF);

  ['ucs2', 'ucs-2', 'utf16le', 'utf-16le'].forEach((encoding) => {
    buf.fill(0xFF);
    assert.strictEqual(buf.write('abcd', 0, 2, encoding), 2);
    assert.strictEqual(buf[0], 0x61);
    assert.strictEqual(buf[1], 0x00);
    assert.strictEqual(buf[2], 0xFF);
    assert.strictEqual(buf[3], 0xFF);
  });
}

{
  // Test offset returns are correct
  const b = Buffer.allocUnsafe(16);
  assert.strictEqual(b.writeUInt32LE(0, 0), 4);
  assert.strictEqual(b.writeUInt16LE(0, 4), 6);
  assert.strictEqual(b.writeUInt8(0, 6), 7);
  assert.strictEqual(b.writeInt8(0, 7), 8);
  assert.strictEqual(b.writeDoubleLE(0, 8), 16);
}

{
  // Test unmatched surrogates not producing invalid utf8 output
  // ef bf bd = utf-8 representation of unicode replacement character
  // see https://codereview.chromium.org/121173009/
  const buf = Buffer.from('ab\ud800cd', 'utf8');
  assert.strictEqual(buf[0], 0x61);
  assert.strictEqual(buf[1], 0x62);
  assert.strictEqual(buf[2], 0xef);
  assert.strictEqual(buf[3], 0xbf);
  assert.strictEqual(buf[4], 0xbd);
  assert.strictEqual(buf[5], 0x63);
  assert.strictEqual(buf[6], 0x64);
}

{
  // Test for buffer overrun
  const buf = Buffer.from([0, 0, 0, 0, 0]); // length: 5
  const sub = buf.slice(0, 4);         // length: 4
  assert.strictEqual(sub.write('12345', 'latin1'), 4);
  assert.strictEqual(buf[4], 0);
  assert.strictEqual(sub.write('12345', 'binary'), 4);
  assert.strictEqual(buf[4], 0);
}

{
  // Test alloc with fill option
  const buf = Buffer.alloc(5, '800A', 'hex');
  assert.strictEqual(buf[0], 128);
  assert.strictEqual(buf[1], 10);
  assert.strictEqual(buf[2], 128);
  assert.strictEqual(buf[3], 10);
  assert.strictEqual(buf[4], 128);
}


// Check for fractional length args, junk length args, etc.
// https://github.com/joyent/node/issues/1758

// Call .fill() first, stops valgrind warning about uninitialized memory reads.
Buffer.allocUnsafe(3.3).fill().toString();
// Throws bad argument error in commit 43cb4ec
Buffer.alloc(3.3).fill().toString();
assert.strictEqual(Buffer.allocUnsafe(3.3).length, 3);
assert.strictEqual(Buffer.from({ length: 3.3 }).length, 3);
assert.strictEqual(Buffer.from({ length: 'BAM' }).length, 0);

// Make sure that strings are not coerced to numbers.
assert.strictEqual(Buffer.from('99').length, 2);
assert.strictEqual(Buffer.from('13.37').length, 5);

// Ensure that the length argument is respected.
['ascii', 'utf8', 'hex', 'base64', 'latin1', 'binary'].forEach((enc) => {
  assert.strictEqual(Buffer.allocUnsafe(1).write('aaaaaa', 0, 1, enc), 1);
});

{
  // Regression test, guard against buffer overrun in the base64 decoder.
  const a = Buffer.allocUnsafe(3);
  const b = Buffer.from('xxx');
  a.write('aaaaaaaa', 'base64');
  assert.strictEqual(b.toString(), 'xxx');
}

// issue GH-3416
Buffer.from(Buffer.allocUnsafe(0), 0, 0);

// issue GH-5587
assert.throws(
  () => Buffer.alloc(8).writeFloatLE(0, 5),
  outOfRangeError
);
assert.throws(
  () => Buffer.alloc(16).writeDoubleLE(0, 9),
  outOfRangeError
);

// Attempt to overflow buffers, similar to previous bug in array buffers
assert.throws(
  () => Buffer.allocUnsafe(8).writeFloatLE(0.0, 0xffffffff),
  outOfRangeError
);
assert.throws(
  () => Buffer.allocUnsafe(8).writeFloatLE(0.0, 0xffffffff),
  outOfRangeError
);

// Ensure negative values can't get past offset
assert.throws(
  () => Buffer.allocUnsafe(8).writeFloatLE(0.0, -1),
  outOfRangeError
);
assert.throws(
  () => Buffer.allocUnsafe(8).writeFloatLE(0.0, -1),
  outOfRangeError
);

// Test for common write(U)IntLE/BE
{
  let buf = Buffer.allocUnsafe(3);
  buf.writeUIntLE(0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x56, 0x34, 0x12]);
  assert.strictEqual(buf.readUIntLE(0, 3), 0x123456);

  buf.fill(0xFF);
  buf.writeUIntBE(0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x12, 0x34, 0x56]);
  assert.strictEqual(buf.readUIntBE(0, 3), 0x123456);

  buf.fill(0xFF);
  buf.writeIntLE(0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x56, 0x34, 0x12]);
  assert.strictEqual(buf.readIntLE(0, 3), 0x123456);

  buf.fill(0xFF);
  buf.writeIntBE(0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x12, 0x34, 0x56]);
  assert.strictEqual(buf.readIntBE(0, 3), 0x123456);

  buf.fill(0xFF);
  buf.writeIntLE(-0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0xaa, 0xcb, 0xed]);
  assert.strictEqual(buf.readIntLE(0, 3), -0x123456);

  buf.fill(0xFF);
  buf.writeIntBE(-0x123456, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0xed, 0xcb, 0xaa]);
  assert.strictEqual(buf.readIntBE(0, 3), -0x123456);

  buf.fill(0xFF);
  buf.writeIntLE(-0x123400, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x00, 0xcc, 0xed]);
  assert.strictEqual(buf.readIntLE(0, 3), -0x123400);

  buf.fill(0xFF);
  buf.writeIntBE(-0x123400, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0xed, 0xcc, 0x00]);
  assert.strictEqual(buf.readIntBE(0, 3), -0x123400);

  buf.fill(0xFF);
  buf.writeIntLE(-0x120000, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0x00, 0x00, 0xee]);
  assert.strictEqual(buf.readIntLE(0, 3), -0x120000);

  buf.fill(0xFF);
  buf.writeIntBE(-0x120000, 0, 3);
  assert.deepStrictEqual(buf.toJSON().data, [0xee, 0x00, 0x00]);
  assert.strictEqual(buf.readIntBE(0, 3), -0x120000);

  buf = Buffer.allocUnsafe(5);
  buf.writeUIntLE(0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x90, 0x78, 0x56, 0x34, 0x12]);
  assert.strictEqual(buf.readUIntLE(0, 5), 0x1234567890);

  buf.fill(0xFF);
  buf.writeUIntBE(0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x12, 0x34, 0x56, 0x78, 0x90]);
  assert.strictEqual(buf.readUIntBE(0, 5), 0x1234567890);

  buf.fill(0xFF);
  buf.writeIntLE(0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x90, 0x78, 0x56, 0x34, 0x12]);
  assert.strictEqual(buf.readIntLE(0, 5), 0x1234567890);

  buf.fill(0xFF);
  buf.writeIntBE(0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x12, 0x34, 0x56, 0x78, 0x90]);
  assert.strictEqual(buf.readIntBE(0, 5), 0x1234567890);

  buf.fill(0xFF);
  buf.writeIntLE(-0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x70, 0x87, 0xa9, 0xcb, 0xed]);
  assert.strictEqual(buf.readIntLE(0, 5), -0x1234567890);

  buf.fill(0xFF);
  buf.writeIntBE(-0x1234567890, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0xed, 0xcb, 0xa9, 0x87, 0x70]);
  assert.strictEqual(buf.readIntBE(0, 5), -0x1234567890);

  buf.fill(0xFF);
  buf.writeIntLE(-0x0012000000, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0x00, 0x00, 0x00, 0xee, 0xff]);
  assert.strictEqual(buf.readIntLE(0, 5), -0x0012000000);

  buf.fill(0xFF);
  buf.writeIntBE(-0x0012000000, 0, 5);
  assert.deepStrictEqual(buf.toJSON().data, [0xff, 0xee, 0x00, 0x00, 0x00]);
  assert.strictEqual(buf.readIntBE(0, 5), -0x0012000000);
}

// Regression test for https://github.com/nodejs/node-v0.x-archive/issues/5482:
// should throw but not assert in C++ land.
assert.throws(
  () => Buffer.from('', 'buffer'),
  {
    code: 'ERR_UNKNOWN_ENCODING',
    name: 'TypeError',
    message: 'Unknown encoding: buffer'
  }
);

// Regression test for https://github.com/nodejs/node-v0.x-archive/issues/6111.
// Constructing a buffer from another buffer should a) work, and b) not corrupt
// the source buffer.
{
  const a = [...Array(128).keys()]; // [0, 1, 2, 3, ... 126, 127]
  const b = Buffer.from(a);
  const c = Buffer.from(b);
  assert.strictEqual(b.length, a.length);
  assert.strictEqual(c.length, a.length);
  for (let i = 0, k = a.length; i < k; ++i) {
    assert.strictEqual(a[i], i);
    assert.strictEqual(b[i], i);
    assert.strictEqual(c[i], i);
  }
}

if (common.hasCrypto) { // eslint-disable-line node-core/crypto-check
  // Test truncation after decode
  const crypto = require('crypto');

  const b1 = Buffer.from('YW55=======', 'base64');
  const b2 = Buffer.from('YW55', 'base64');

  assert.strictEqual(
    crypto.createHash('sha1').update(b1).digest('hex'),
    crypto.createHash('sha1').update(b2).digest('hex')
  );
} else {
  common.printSkipMessage('missing crypto');
}

const ps = Buffer.poolSize;
Buffer.poolSize = 0;
assert(Buffer.allocUnsafe(1).parent instanceof ArrayBuffer);
Buffer.poolSize = ps;

assert.throws(
  () => Buffer.allocUnsafe(10).copy(),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "target" argument must be an instance of Buffer or ' +
             'Uint8Array. Received undefined'
  });

assert.throws(() => Buffer.from(), {
  name: 'TypeError',
  message: 'The first argument must be of type string or an instance of ' +
  'Buffer, ArrayBuffer, or Array or an Array-like Object. Received undefined'
});
assert.throws(() => Buffer.from(null), {
  name: 'TypeError',
  message: 'The first argument must be of type string or an instance of ' +
  'Buffer, ArrayBuffer, or Array or an Array-like Object. Received null'
});

// Test prototype getters don't throw
assert.strictEqual(Buffer.prototype.parent, undefined);
assert.strictEqual(Buffer.prototype.offset, undefined);
assert.strictEqual(SlowBuffer.prototype.parent, undefined);
assert.strictEqual(SlowBuffer.prototype.offset, undefined);


{
  // Test that large negative Buffer length inputs don't affect the pool offset.
  // Use the fromArrayLike() variant here because it's more lenient
  // about its input and passes the length directly to allocate().
  assert.deepStrictEqual(Buffer.from({ length: -Buffer.poolSize }),
                         Buffer.from(''));
  assert.deepStrictEqual(Buffer.from({ length: -100 }),
                         Buffer.from(''));

  // Check pool offset after that by trying to write string into the pool.
  Buffer.from('abc');
}


// Test that ParseArrayIndex handles full uint32
{
  const errMsg = common.expectsError({
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"offset" is outside of buffer bounds'
  });
  assert.throws(() => Buffer.from(new ArrayBuffer(0), -1 >>> 0), errMsg);
}

// ParseArrayIndex() should reject values that don't fit in a 32 bits size_t.
assert.throws(() => {
  const a = Buffer.alloc(1);
  const b = Buffer.alloc(1);
  a.copy(b, 0, 0x100000000, 0x100000001);
}, outOfRangeError);

// Unpooled buffer (replaces SlowBuffer)
{
  const ubuf = Buffer.allocUnsafeSlow(10);
  assert(ubuf);
  assert(ubuf.buffer);
  assert.strictEqual(ubuf.buffer.byteLength, 10);
}

// Regression test to verify that an empty ArrayBuffer does not throw.
Buffer.from(new ArrayBuffer());

// Test that ArrayBuffer from a different context is detected correctly.
const arrayBuf = vm.runInNewContext('new ArrayBuffer()');
Buffer.from(arrayBuf);
Buffer.from({ buffer: arrayBuf });

assert.throws(() => Buffer.alloc({ valueOf: () => 1 }),
              /"size" argument must be of type number/);
assert.throws(() => Buffer.alloc({ valueOf: () => -1 }),
              /"size" argument must be of type number/);

assert.strictEqual(Buffer.prototype.toLocaleString, Buffer.prototype.toString);
{
  const buf = Buffer.from('test');
  assert.strictEqual(buf.toLocaleString(), buf.toString());
}

assert.throws(() => {
  Buffer.alloc(0x1000, 'This is not correctly encoded', 'hex');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError'
});

assert.throws(() => {
  Buffer.alloc(0x1000, 'c', 'hex');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError'
});

assert.throws(() => {
  Buffer.alloc(1, Buffer.alloc(0));
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError'
});

assert.throws(() => {
  Buffer.alloc(40, 'x', 20);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
});
