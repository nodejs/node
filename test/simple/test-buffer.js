require("../common");
assert = require("assert");

var Buffer = require('buffer').Buffer;

var b = new Buffer(1024);

puts("b.length == " + b.length);
assert.equal(1024, b.length);

for (var i = 0; i < 1024; i++) {
  assert.ok(b[i] >= 0);
  b[i] = i % 256;
}

for (var i = 0; i < 1024; i++) {
  assert.equal(i % 256, b[i]);
}

var c = new Buffer(512);

var copied = b.copy(c, 0, 0, 512);
assert.equal(512, copied);
for (var i = 0; i < c.length; i++) {
  print('.');
  assert.equal(i % 256, c[i]);
}


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
puts('bytes written to buffer: ' + size);
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