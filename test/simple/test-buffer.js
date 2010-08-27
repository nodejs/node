common = require("../common");
assert = common.assert
assert = require("assert");

var Buffer = require('buffer').Buffer;

var b = Buffer(1024); // safe constructor

console.log("b.length == " + b.length);
assert.strictEqual(1024, b.length);

for (var i = 0; i < 1024; i++) {
  assert.ok(b[i] >= 0);
  b[i] = i % 256;
}

for (var i = 0; i < 1024; i++) {
  assert.equal(i % 256, b[i]);
}

var c = new Buffer(512);

// copy 512 bytes, from 0 to 512.
var copied = b.copy(c, 0, 0, 512);
console.log("copied " + copied + " bytes from b into c");
assert.strictEqual(512, copied);
for (var i = 0; i < c.length; i++) {
  common.print('.');
  assert.equal(i % 256, c[i]);
}
console.log("");

// try to copy 513 bytes, and hope we don't overrun c, which is only 512 long
var copied = b.copy(c, 0, 0, 513);
console.log("copied " + copied + " bytes from b into c");
assert.strictEqual(512, copied);
for (var i = 0; i < c.length; i++) {
  assert.equal(i % 256, c[i]);
}

// copy all of c back into b, without specifying sourceEnd
var copied = c.copy(b, 0, 0);
console.log("copied " + copied + " bytes from c back into b");
assert.strictEqual(512, copied);
for (var i = 0; i < b.length; i++) {
  assert.equal(i % 256, b[i]);
}

// copy 768 bytes from b into b
var copied = b.copy(b, 0, 256, 1024);
console.log("copied " + copied + " bytes from b into c");
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
assert.equal(new Buffer('abc').toString({toString: function () {return 'ascii';}}), 'abc');

// testing for smart defaults and ability to pass string values as offset
var writeTest = new Buffer('abcdes');
writeTest.write('n', 'ascii');
writeTest.write('o', 'ascii', '1');
writeTest.write('d', '2', 'ascii');
writeTest.write('e', 3, 'ascii');
writeTest.write('j', 'ascii', 4);
assert.equal(writeTest.toString(), 'nodejs');

var asciiString = "hello world";
var offset = 100;
for (var j = 0; j < 500; j++) {

  for (var i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }
  var asciiSlice = b.toString('ascii', 0, asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var written = b.asciiWrite(asciiString, offset);
  assert.equal(asciiString.length, written);
  var asciiSlice = b.toString('ascii', offset, offset+asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var sliceA = b.slice(offset, offset+asciiString.length);
  var sliceB = b.slice(offset, offset+asciiString.length);
  for (var i = 0; i < asciiString.length; i++) {
    assert.equal(sliceA[i], sliceB[i]);
  }

  // TODO utf8 slice tests
}


for (var j = 0; j < 100; j++) {
  var slice = b.slice(100, 150);
  assert.equal(50, slice.length);
  for (var i = 0; i < 50; i++) {
    assert.equal(b[100+i], slice[i]);
  }
}


// unpack

var b = new Buffer(10);
b[0] = 0x00;
b[1] = 0x01;
b[2] = 0x03;
b[3] = 0x00;

assert.deepEqual([0x0001], b.unpack('n', 0));
assert.deepEqual([0x0001, 0x0300], b.unpack('nn', 0));
assert.deepEqual([0x0103], b.unpack('n', 1));
assert.deepEqual([0x0300], b.unpack('n', 2));
assert.deepEqual([0x00010300], b.unpack('N', 0));
assert.throws(function () {
  b.unpack('N', 8);
});

b[4] = 0xDE;
b[5] = 0xAD;
b[6] = 0xBE;
b[7] = 0xEF;

assert.deepEqual([0xDEADBEEF], b.unpack('N', 4));


// Bug regression test
var testValue = '\u00F6\u65E5\u672C\u8A9E'; // ö日本語
var buffer = new Buffer(32);
var size = buffer.utf8Write(testValue, 0);
console.log('bytes written to buffer: ' + size);
var slice = buffer.toString('utf8', 0, size);
assert.equal(slice, testValue);


// Test triple  slice
var a = new Buffer(8);
for (var i = 0; i < 8; i++) a[i] = i;
var b = a.slice(4,8);
assert.equal(4, b[0]);
assert.equal(5, b[1]);
assert.equal(6, b[2]);
assert.equal(7, b[3]);
var c = b.slice(2 , 4);
assert.equal(6, c[0]);
assert.equal(7, c[1]);


var d = new Buffer([23, 42, 255]);
assert.equal(d.length, 3);
assert.equal(d[0], 23);
assert.equal(d[1], 42);
assert.equal(d[2], 255);

var e = new Buffer('über');
assert.deepEqual(e, new Buffer([195, 188, 98, 101, 114]));

var f = new Buffer('über', 'ascii');
assert.deepEqual(f, new Buffer([252, 98, 101, 114]));


//
// Test toString('base64')
//
assert.equal('TWFu', (new Buffer('Man')).toString('base64'));
// big example
quote = "Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure.";
expected = "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=";
assert.equal(expected, (new Buffer(quote)).toString('base64'));


b = new Buffer(1024);
bytesWritten = b.write(expected, 0, 'base64');
assert.equal(quote, b.toString('ascii', 0, quote.length));
assert.equal(quote.length, bytesWritten);

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
assert.equal(new Buffer('KioqKioqKioqKg==', 'base64').toString(), '**********');
assert.equal(new Buffer('KioqKioqKioqKio=', 'base64').toString(), '***********');
assert.equal(new Buffer('KioqKioqKioqKioq', 'base64').toString(), '************');
assert.equal(new Buffer('KioqKioqKioqKioqKg==', 'base64').toString(), '*************');
assert.equal(new Buffer('KioqKioqKioqKioqKio=', 'base64').toString(), '**************');
assert.equal(new Buffer('KioqKioqKioqKioqKioq', 'base64').toString(), '***************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKg==', 'base64').toString(), '****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKio=', 'base64').toString(), '*****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioq', 'base64').toString(), '******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKg==', 'base64').toString(), '*******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKio=', 'base64').toString(), '********************');

// no padding, not a multiple of 4
assert.equal(new Buffer('Kg', 'base64').toString(), '*');
assert.equal(new Buffer('Kio', 'base64').toString(), '**');
assert.equal(new Buffer('KioqKg', 'base64').toString(), '****');
assert.equal(new Buffer('KioqKio', 'base64').toString(), '*****');
assert.equal(new Buffer('KioqKioqKg', 'base64').toString(), '*******');
assert.equal(new Buffer('KioqKioqKio', 'base64').toString(), '********');
assert.equal(new Buffer('KioqKioqKioqKg', 'base64').toString(), '**********');
assert.equal(new Buffer('KioqKioqKioqKio', 'base64').toString(), '***********');
assert.equal(new Buffer('KioqKioqKioqKioqKg', 'base64').toString(), '*************');
assert.equal(new Buffer('KioqKioqKioqKioqKio', 'base64').toString(), '**************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKg', 'base64').toString(), '****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKio', 'base64').toString(), '*****************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKg', 'base64').toString(), '*******************');
assert.equal(new Buffer('KioqKioqKioqKioqKioqKioqKio', 'base64').toString(), '********************');

// handle padding graciously, multiple-of-4 or not
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw==', 'base64').length, 32);
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw=',  'base64').length, 32);
assert.equal(new Buffer('72INjkR5fchcxk9+VgdGPFJDxUBFR5/rMFsghgxADiw',   'base64').length, 32);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg==',  'base64').length, 31);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg=',   'base64').length, 31);
assert.equal(new Buffer('w69jACy6BgZmaFvv96HG6MYksWytuZu3T1FvGnulPg',    'base64').length, 31);

// This string encodes single '.' character in UTF-16
dot = new Buffer('//4uAA==', 'base64');
assert.equal(dot[0], 0xff);
assert.equal(dot[1], 0xfe);
assert.equal(dot[2], 0x2e);
assert.equal(dot[3], 0x00);
assert.equal(dot.toString('base64'), '//4uAA==');
