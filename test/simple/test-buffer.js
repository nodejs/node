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

var asciiString = "hello world";
var offset = 100;
for (var j = 0; j < 500; j++) {

  for (var i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }
  var asciiSlice = b.asciiSlice(0, asciiString.length);
  assert.equal(asciiString, asciiSlice);

  var written = b.asciiWrite(asciiString, offset);
  assert.equal(asciiString.length, written);
  var asciiSlice = b.asciiSlice(offset, offset+asciiString.length);
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
