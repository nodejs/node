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

var e = new Buffer('über');
console.error('uber: \'%s\'', e.toString());
assert.deepEqual(e, new Buffer([195, 188, 98, 101, 114]));

var f = new Buffer('über', 'ascii');
console.error('f.length: %d     (should be 4)', f.length);
assert.deepEqual(f, new Buffer([252, 98, 101, 114]));


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
assert.equal(12, Buffer.byteLength('Il était tué', 'ascii'));
assert.equal(12, Buffer.byteLength('Il était tué', 'binary'));


// slice(0,0).length === 0
assert.equal(0, Buffer('hello').slice(0, 0).length);
