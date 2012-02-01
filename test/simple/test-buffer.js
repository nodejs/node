// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var Buffer = require('buffer').Buffer;

var b = Buffer(1024); // safe constructor

console.log('b.length == ' + b.length);
assert.strictEqual(1024, b.length);

b[0] = -1;
assert.equal(b[0], 255);

for (var i = 0; i < 1024; i++) {
  b[i] = i % 256;
}

for (var i = 0; i < 1024; i++) {
  assert.equal(i % 256, b[i]);
}

var c = new Buffer(512);
console.log('c.length == %d', c.length);
assert.strictEqual(512, c.length);

// copy 512 bytes, from 0 to 512.
var copied = b.copy(c, 0, 0, 512);
console.log('copied ' + copied + ' bytes from b into c');
assert.equal(512, copied);
for (var i = 0; i < c.length; i++) {
  common.print('.');
  assert.equal(i % 256, c[i]);
}
console.log('');

// try to copy 513 bytes, and hope we don't overrun c, which is only 512 long
var copied = b.copy(c, 0, 0, 513);
console.log('copied ' + copied + ' bytes from b into c');
assert.strictEqual(512, copied);
for (var i = 0; i < c.length; i++) {
  assert.equal(i % 256, c[i]);
}

// copy all of c back into b, without specifying sourceEnd
var copied = c.copy(b, 0, 0);
console.log('copied ' + copied + ' bytes from c back into b');
assert.strictEqual(512, copied);
for (var i = 0; i < b.length; i++) {
  assert.equal(i % 256, b[i]);
}

// copy 768 bytes from b into b
var copied = b.copy(b, 0, 256, 1024);
console.log('copied ' + copied + ' bytes from b into c');
assert.strictEqual(768, copied);
for (var i = 0; i < c.length; i++) {
  assert.equal(i % 256, c[i]);
}

var caught_error = null;

// try to copy from before the beginning of b
caught_error = null;
try {
  var copied = b.copy(c, 0, 100, 10);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('sourceEnd < sourceStart', caught_error.message);

// try to copy to before the beginning of c
caught_error = null;
try {
  var copied = b.copy(c, -1, 0, 10);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('targetStart out of bounds', caught_error.message);

// try to copy to after the end of c
caught_error = null;
try {
  var copied = b.copy(c, 512, 0, 10);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('targetStart out of bounds', caught_error.message);

// try to copy starting before the beginning of b
caught_error = null;
try {
  var copied = b.copy(c, 0, -1, 1);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('sourceStart out of bounds', caught_error.message);

// try to copy starting after the end of b
caught_error = null;
try {
  var copied = b.copy(c, 0, 1024, 1025);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('sourceStart out of bounds', caught_error.message);

// a too-low sourceEnd will get caught by earlier checks

// try to copy ending after the end of b
try {
  var copied = b.copy(c, 0, 1023, 1025);
} catch (err) {
  caught_error = err;
}
assert.strictEqual('sourceEnd out of bounds', caught_error.message);

// try to create 0-length buffers
new Buffer('');
new Buffer('', 'ascii');
new Buffer('', 'binary');
new Buffer(0);

// try to write a 0-length string beyond the end of b
b.write('', 1024);
b.write('', 2048);

// try to copy 0 bytes worth of data into an empty buffer
b.copy(new Buffer(0), 0, 0, 0);

// try to copy 0 bytes past the end of the target buffer
b.copy(new Buffer(0), 1, 1, 1);
b.copy(new Buffer(1), 1, 1, 1);

// try to copy 0 bytes from past the end of the source buffer
b.copy(new Buffer(1), 0, 2048, 2048);

// try to toString() a 0-length slice of a buffer, both within and without the
// valid buffer range
assert.equal(new Buffer('abc').toString('ascii', 0, 0), '');
assert.equal(new Buffer('abc').toString('ascii', -100, -100), '');
assert.equal(new Buffer('abc').toString('ascii', 100, 100), '');

// try toString() with a object as a encoding
assert.equal(new Buffer('abc').toString({toString: function() {
  return 'ascii';
}}), 'abc');

// testing for smart defaults and ability to pass string values as offset
var writeTest = new Buffer('abcdes');
writeTest.write('n', 'ascii');
writeTest.write('o', 'ascii', '1');
writeTest.write('d', '2', 'ascii');
writeTest.write('e', 3, 'ascii');
writeTest.write('j', 'ascii', 4);
assert.equal(writeTest.toString(), 'nodejs');

var asciiString = 'hello world';
var offset = 100;
for (var j = 0; j < 500; j++) {

  for (var i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }
  var asciiSlice = b.toString('ascii', 0, asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var written = b.write(asciiString, offset, 'ascii');
  assert.equal(asciiString.length, written);
  var asciiSlice = b.toString('ascii', offset, offset + asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var sliceA = b.slice(offset, offset + asciiString.length);
  var sliceB = b.slice(offset, offset + asciiString.length);
  for (var i = 0; i < asciiString.length; i++) {
    assert.equal(sliceA[i], sliceB[i]);
  }

  // TODO utf8 slice tests
}


for (var j = 0; j < 100; j++) {
  var slice = b.slice(100, 150);
  assert.equal(50, slice.length);
  for (var i = 0; i < 50; i++) {
    assert.equal(b[100 + i], slice[i]);
  }
}



// Bug regression test
var testValue = '\u00F6\u65E5\u672C\u8A9E'; // ö日本語
var buffer = new Buffer(32);
var size = buffer.write(testValue, 0, 'utf8');
console.log('bytes written to buffer: ' + size);
var slice = buffer.toString('utf8', 0, size);
assert.equal(slice, testValue);


// Test triple  slice
var a = new Buffer(8);
for (var i = 0; i < 8; i++) a[i] = i;
var b = a.slice(4, 8);
assert.equal(4, b[0]);
assert.equal(5, b[1]);
assert.equal(6, b[2]);
assert.equal(7, b[3]);
var c = b.slice(2, 4);
assert.equal(6, c[0]);
assert.equal(7, c[1]);


var d = new Buffer([23, 42, 255]);
assert.equal(d.length, 3);
assert.equal(d[0], 23);
assert.equal(d[1], 42);
assert.equal(d[2], 255);
assert.deepEqual(d, new Buffer(d));

var e = new Buffer('über');
console.error('uber: \'%s\'', e.toString());
assert.deepEqual(e, new Buffer([195, 188, 98, 101, 114]));

var f = new Buffer('über', 'ascii');
console.error('f.length: %d     (should be 4)', f.length);
assert.deepEqual(f, new Buffer([252, 98, 101, 114]));

var f = new Buffer('über', 'ucs2');
console.error('f.length: %d     (should be 8)', f.length);
assert.deepEqual(f, new Buffer([252, 0, 98, 0, 101, 0, 114, 0]));

var f = new Buffer('привет', 'ucs2');
console.error('f.length: %d     (should be 12)', f.length);
assert.deepEqual(f, new Buffer([63, 4, 64, 4, 56, 4, 50, 4, 53, 4, 66, 4]));
assert.equal(f.toString('ucs2'), 'привет');

var f = new Buffer([0, 0, 0, 0, 0]);
assert.equal(f.length, 5);
var size = f.write('あいうえお', 'ucs2');
var charsWritten = Buffer._charsWritten; // Copy value out.
console.error('bytes written to buffer: %d     (should be 4)', size);
console.error('chars written to buffer: %d     (should be 2)', charsWritten);
assert.equal(size, 4);
assert.equal(charsWritten, 2);
assert.deepEqual(f, new Buffer([0x42, 0x30, 0x44, 0x30, 0x00]));


var arrayIsh = {0: 0, 1: 1, 2: 2, 3: 3, length: 4};
var g = new Buffer(arrayIsh);
assert.deepEqual(g, new Buffer([0, 1, 2, 3]));
var strArrayIsh = {0: '0', 1: '1', 2: '2', 3: '3', length: 4};
g = new Buffer(strArrayIsh);
assert.deepEqual(g, new Buffer([0, 1, 2, 3]));


//
// Test toString('base64')
//
assert.equal('TWFu', (new Buffer('Man')).toString('base64'));
// big example
var quote = 'Man is distinguished, not only by his reason, but by this ' +
            'singular passion from other animals, which is a lust ' +
            'of the mind, that by a perseverance of delight in the continued ' +
            'and indefatigable generation of knowledge, exceeds the short ' +
            'vehemence of any carnal pleasure.';
var expected = 'TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24s' +
               'IGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltY' +
               'WxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZX' +
               'JzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmR' +
               'lZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo' +
               'ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=';
assert.equal(expected, (new Buffer(quote)).toString('base64'));


b = new Buffer(1024);
var bytesWritten = b.write(expected, 0, 'base64');
assert.equal(quote.length, bytesWritten);
assert.equal(quote, b.toString('ascii', 0, quote.length));

// check that the base64 decoder ignores whitespace
var expectedWhite = expected.slice(0, 60) + ' \n' +
                    expected.slice(60, 120) + ' \n' +
                    expected.slice(120, 180) + ' \n' +
                    expected.slice(180, 240) + ' \n' +
                    expected.slice(240, 300) + '\n' +
                    expected.slice(300, 360) + '\n';
b = new Buffer(1024);
bytesWritten = b.write(expectedWhite, 0, 'base64');
assert.equal(quote.length, bytesWritten);
assert.equal(quote, b.toString('ascii', 0, quote.length));

// check that the base64 decoder on the constructor works
// even in the presence of whitespace.
b = new Buffer(expectedWhite, 'base64');
assert.equal(quote.length, b.length);
assert.equal(quote, b.toString('ascii', 0, quote.length));

// check that the base64 decoder ignores illegal chars
var expectedIllegal = expected.slice(0, 60) + ' \x80' +
                      expected.slice(60, 120) + ' \xff' +
                      expected.slice(120, 180) + ' \x00' +
                      expected.slice(180, 240) + ' \x98' +
                      expected.slice(240, 300) + '\x03' +
                      expected.slice(300, 360);
b = new Buffer(expectedIllegal, 'base64');
assert.equal(quote.length, b.length);
assert.equal(quote, b.toString('ascii', 0, quote.length));


assert.equal(new Buffer('', 'base64').toString(), '');
assert.equal(new Buffer('K', 'base64').toString(), '');

// multiple-of-4 with padding
assert.equal(new Buffer('Kg==', 'base64').toString(), '*');
assert.equal(new Buffer('Kio=', 'base64').toString(), '**');
assert.equal(new Buffer('Kioq', 'base64').toString(), '***');
assert.equal(new Buffer('KioqKg==', 'base64').toString(), '****');
assert.equal(new Buffer('KioqKio=', 'base64').toString(), '*****');
assert.equal(new Buffer('KioqKioq', 'base64').toString(), '******');
assert.equal(new Buffer('KioqKioqKg==', 'base64').toString(), '*******');
assert.equal(new Buffer('KioqKioqKio=', 'base64').toString(), '********');
assert.equal(new Buffer('KioqKioqKioq', 'base64').toString(), '*********');
assert.equal(new Buffer('KioqKioqKioqKg==', 'base64').toString(),
             '**********');
assert.equal(new Buffer('KioqKioqKioqKio=', 'base64').toString(),
             '***********');
assert.equal(new Buffer('KioqKioqKioqKioq', 'base64').toString(),
             '************');
assert.equal(new Buffer('KioqKioqKioqKioqKg==', 'base64').toString(),
             '*************');
assert.equal(new Buffer('KioqKioqKioqKioqKio=', 'base64').toString(),
             '**************');
assert.equal(new Buffer('KioqKioqKioqKioqKioq', 'base64').toString(),
             '***************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKg==', 'base64').toString(),
             '****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKio=', 'base64').toString(),
             '*****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioq', 'base64').toString(),
             '******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKg==', 'base64').toString(),
             '*******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKio=', 'base64').toString(),
             '********************');

// no padding, not a multiple of 4
assert.equal(new Buffer('Kg', 'base64').toString(), '*');
assert.equal(new Buffer('Kio', 'base64').toString(), '**');
assert.equal(new Buffer('KioqKg', 'base64').toString(), '****');
assert.equal(new Buffer('KioqKio', 'base64').toString(), '*****');
assert.equal(new Buffer('KioqKioqKg', 'base64').toString(), '*******');
assert.equal(new Buffer('KioqKioqKio', 'base64').toString(), '********');
assert.equal(new Buffer('KioqKioqKioqKg', 'base64').toString(), '**********');
assert.equal(new Buffer('KioqKioqKioqKio', 'base64').toString(), '***********');
assert.equal(new Buffer('KioqKioqKioqKioqKg', 'base64').toString(),
             '*************');
assert.equal(new Buffer('KioqKioqKioqKioqKio', 'base64').toString(),
             '**************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKg', 'base64').toString(),
             '****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKio', 'base64').toString(),
             '*****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKg', 'base64').toString(),
             '*******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKio', 'base64').toString(),
             '********************');

// handle padding graciously, multiple-of-4 or not
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw==',
                        'base64').length, 32);
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw=',
                        'base64').length, 32);
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw',
                        'base64').length, 32);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg==',
                        'base64').length, 31);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg=',
                        'base64').length, 31);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg',
                        'base64').length, 31);

// This string encodes single '.' character in UTF-16
var dot = new Buffer('//4uAA==', 'base64');
assert.equal(dot[0], 0xff);
assert.equal(dot[1], 0xfe);
assert.equal(dot[2], 0x2e);
assert.equal(dot[3], 0x00);
assert.equal(dot.toString('base64'), '//4uAA==');

// Writing base64 at a position > 0 should not mangle the result.
//
// https://github.com/joyent/node/issues/402
var segments = ['TWFkbmVzcz8h', 'IFRoaXM=', 'IGlz', 'IG5vZGUuanMh'];
var buf = new Buffer(64);
var pos = 0;

for (var i = 0; i < segments.length; ++i) {
  pos += b.write(segments[i], pos, 'base64');
}
assert.equal(b.toString('binary', 0, pos), 'Madness?! This is node.js!');

// Creating buffers larger than pool size.
var l = Buffer.poolSize + 5;
var s = '';
for (i = 0; i < l; i++) {
  s += 'h';
}

var b = new Buffer(s);

for (i = 0; i < l; i++) {
  assert.equal('h'.charCodeAt(0), b[i]);
}

var sb = b.toString();
assert.equal(sb.length, s.length);
assert.equal(sb, s);


// Single argument slice
b = new Buffer('abcde');
assert.equal('bcde', b.slice(1).toString());

// byte length
assert.equal(14, Buffer.byteLength('Il était tué'));
assert.equal(14, Buffer.byteLength('Il était tué', 'utf8'));
assert.equal(24, Buffer.byteLength('Il était tué', 'ucs2'));
assert.equal(12, Buffer.byteLength('Il était tué', 'ascii'));
assert.equal(12, Buffer.byteLength('Il était tué', 'binary'));

// slice(0,0).length === 0
assert.equal(0, Buffer('hello').slice(0, 0).length);

// test hex toString
console.log('Create hex string from buffer');
var hexb = new Buffer(256);
for (var i = 0; i < 256; i++) {
  hexb[i] = i;
}
var hexStr = hexb.toString('hex');
assert.equal(hexStr,
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

console.log('Create buffer from hex string');
var hexb2 = new Buffer(hexStr, 'hex');
for (var i = 0; i < 256; i++) {
  assert.equal(hexb2[i], hexb[i]);
}

// test an invalid slice end.
console.log('Try to slice off the end of the buffer');
var b = new Buffer([1, 2, 3, 4, 5]);
var b2 = b.toString('hex', 1, 10000);
var b3 = b.toString('hex', 1, 5);
var b4 = b.toString('hex', 1);
assert.equal(b2, b3);
assert.equal(b2, b4);


// Test slice on SlowBuffer GH-843
var SlowBuffer = process.binding('buffer').SlowBuffer;

function buildSlowBuffer(data) {
  if (Array.isArray(data)) {
    var buffer = new SlowBuffer(data.length);
    data.forEach(function(v, k) {
      buffer[k] = v;
    });
    return buffer;
  }
  return null;
}

var x = buildSlowBuffer([0x81, 0xa3, 0x66, 0x6f, 0x6f, 0xa3, 0x62, 0x61, 0x72]);

console.log(x.inspect());
assert.equal('<SlowBuffer 81 a3 66 6f 6f a3 62 61 72>', x.inspect());

var z = x.slice(4);
console.log(z.inspect());
console.log(z.length);
assert.equal(5, z.length);
assert.equal(0x6f, z[0]);
assert.equal(0xa3, z[1]);
assert.equal(0x62, z[2]);
assert.equal(0x61, z[3]);
assert.equal(0x72, z[4]);

var z = x.slice(0);
console.log(z.inspect());
console.log(z.length);
assert.equal(z.length, x.length);

var z = x.slice(0, 4);
console.log(z.inspect());
console.log(z.length);
assert.equal(4, z.length);
assert.equal(0x81, z[0]);
assert.equal(0xa3, z[1]);

var z = x.slice(0, 9);
console.log(z.inspect());
console.log(z.length);
assert.equal(9, z.length);

var z = x.slice(1, 4);
console.log(z.inspect());
console.log(z.length);
assert.equal(3, z.length);
assert.equal(0xa3, z[0]);

var z = x.slice(2, 4);
console.log(z.inspect());
console.log(z.length);
assert.equal(2, z.length);
assert.equal(0x66, z[0]);
assert.equal(0x6f, z[1]);

assert.equal(0, Buffer('hello').slice(0, 0).length);

b = new Buffer(50);
b.fill('h');
for (var i = 0; i < b.length; i++) {
  assert.equal('h'.charCodeAt(0), b[i]);
}

b.fill(0);
for (var i = 0; i < b.length; i++) {
  assert.equal(0, b[i]);
}

b.fill(1, 16, 32);
for (var i = 0; i < 16; i++) assert.equal(0, b[i]);
for (; i < 32; i++) assert.equal(1, b[i]);
for (; i < b.length; i++) assert.equal(0, b[i]);

var b = new SlowBuffer(10);
b.write('あいうえお', 'ucs2');
assert.equal(b.toString('ucs2'), 'あいうえお');

// Binary encoding should write only one byte per character.
var b = Buffer([0xde, 0xad, 0xbe, 0xef]);
var s = String.fromCharCode(0xffff);
b.write(s, 0, 'binary');
assert.equal(0xff, b[0]);
assert.equal(0xad, b[1]);
assert.equal(0xbe, b[2]);
assert.equal(0xef, b[3]);
s = String.fromCharCode(0xaaee);
b.write(s, 0, 'binary');
assert.equal(0xee, b[0]);
assert.equal(0xad, b[1]);
assert.equal(0xbe, b[2]);
assert.equal(0xef, b[3]);


// This should not segfault the program.
assert.throws(function() {
  new Buffer('"pong"', 0, 6, 8031, '127.0.0.1');
});

// #1210 Test UTF-8 string includes null character
var buf = new Buffer('\0');
assert.equal(buf.length, 1);
buf = new Buffer('\0\0');
assert.equal(buf.length, 2);

buf = new Buffer(2);
var written = buf.write(''); // 0byte
assert.equal(written, 0);
written = buf.write('\0'); // 1byte (v8 adds null terminator)
assert.equal(written, 1);
written = buf.write('a\0'); // 1byte * 2
assert.equal(written, 2);
written = buf.write('あ'); // 3bytes
assert.equal(written, 0);
written = buf.write('\0あ'); // 1byte + 3bytes
assert.equal(written, 1);
written = buf.write('\0\0あ'); // 1byte * 2 + 3bytes
assert.equal(written, 2);

buf = new Buffer(10);
written = buf.write('あいう'); // 3bytes * 3 (v8 adds null terminator)
assert.equal(written, 9);
written = buf.write('あいう\0'); // 3bytes * 3 + 1byte
assert.equal(written, 10);

// #243 Test write() with maxLength
var buf = new Buffer(4);
buf.fill(0xFF);
var written = buf.write('abcd', 1, 2, 'utf8');
console.log(buf);
assert.equal(written, 2);
assert.equal(buf[0], 0xFF);
assert.equal(buf[1], 0x61);
assert.equal(buf[2], 0x62);
assert.equal(buf[3], 0xFF);

buf.fill(0xFF);
written = buf.write('abcd', 1, 4);
console.log(buf);
assert.equal(written, 3);
assert.equal(buf[0], 0xFF);
assert.equal(buf[1], 0x61);
assert.equal(buf[2], 0x62);
assert.equal(buf[3], 0x63);

buf.fill(0xFF);
written = buf.write('abcd', 'utf8', 1, 2);  // legacy style
console.log(buf);
assert.equal(written, 2);
assert.equal(buf[0], 0xFF);
assert.equal(buf[1], 0x61);
assert.equal(buf[2], 0x62);
assert.equal(buf[3], 0xFF);

buf.fill(0xFF);
written = buf.write('abcdef', 1, 2, 'hex');
console.log(buf);
assert.equal(written, 2);
assert.equal(buf[0], 0xFF);
assert.equal(buf[1], 0xAB);
assert.equal(buf[2], 0xCD);
assert.equal(buf[3], 0xFF);

buf.fill(0xFF);
written = buf.write('abcd', 0, 2, 'ucs2');
console.log(buf);
assert.equal(written, 2);
assert.equal(buf[0], 0x61);
assert.equal(buf[1], 0x00);
assert.equal(buf[2], 0xFF);
assert.equal(buf[3], 0xFF);

// test for buffer overrun
buf = new Buffer([0, 0, 0, 0, 0]); // length: 5
var sub = buf.slice(0, 4);         // length: 4
written = sub.write('12345', 'binary');
assert.equal(written, 4);
assert.equal(buf[4], 0);

// test for _charsWritten
buf = new Buffer(9);
buf.write('あいうえ', 'utf8'); // 3bytes * 4
assert.equal(Buffer._charsWritten, 3);
buf.write('あいうえお', 'ucs2'); // 2bytes * 5
assert.equal(Buffer._charsWritten, 4);
buf.write('0123456789', 'ascii');
assert.equal(Buffer._charsWritten, 9);
buf.write('0123456789', 'binary');
assert.equal(Buffer._charsWritten, 9);
buf.write('123456', 'base64');
assert.equal(Buffer._charsWritten, 4);
buf.write('00010203040506070809', 'hex');
assert.equal(Buffer._charsWritten, 18);

// Check for fractional length args, junk length args, etc.
// https://github.com/joyent/node/issues/1758
Buffer(3.3).toString(); // throws bad argument error in commit 43cb4ec
assert.equal(Buffer(-1).length, 0);
assert.equal(Buffer(NaN).length, 0);
assert.equal(Buffer(3.3).length, 4);
assert.equal(Buffer({length: 3.3}).length, 4);
assert.equal(Buffer({length: 'BAM'}).length, 0);

// Make sure that strings are not coerced to numbers.
assert.equal(Buffer('99').length, 2);
assert.equal(Buffer('13.37').length, 5);

// Ensure that the length argument is respected.
'ascii utf8 hex base64 binary'.split(' ').forEach(function(enc) {
  assert.equal(Buffer(1).write('aaaaaa', 0, 1, enc), 1);
});

// Regression test, guard against buffer overrun in the base64 decoder.
var a = Buffer(3);
var b = Buffer('xxx');
a.write('aaaaaaaa', 'base64');
assert.equal(b.toString(), 'xxx');
