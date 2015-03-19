var common = require('../common');
var assert = require('assert');

var tests = [
  testBasic,
  testBounds,
  testOverflow,
  testNonAligned,
  testReturnValue
]

tests.forEach(function(fn) {
  fn();
});

function referenceImplementation(source, maskNum, output, offset, start, end) {
  var i = 0;
  var mask = new Buffer(4);
  var length = end - start;
  mask.writeUInt32LE(maskNum, 0);
  var toCopy = Math.min(length, output.length - offset);
  var maskIdx = 0;
  for (var i = 0; i < toCopy; ++i) {
    output[i + offset] = source[i + start] ^ mask[maskIdx];
    maskIdx = (maskIdx + 1) & 3;
  }
}

function testBasic() {
  var input = new Buffer(256);
  var output = new Buffer(256);
  var refoutput = new Buffer(256);
  var mask = 0xF0F0F0F0;

  for (var i = 0; i < input.length; ++i)
    input[i] = i;

  output[0] = refoutput[0] = 0;
  referenceImplementation(input, mask, refoutput, 1, 0, 255);
  input.mask(mask, output, 1, 0, 255);
  for (var i = 0; i < input.length; ++i) {
    assert.equal(output[i], refoutput[i]);
  }
}

function testBounds() {
  var input = new Buffer(16);
  var output = new Buffer(32);
  try {
    input.mask(120120, output, 0, 0, 17);
    assert.fail('expected error');
  } catch(err) {
    assert.ok(err);
  }
}

function testOverflow() {
  var input = new Buffer(16);
  var output = new Buffer(15);
  try {
    input.mask(120120, output);
    assert.fail('expected error');
  } catch(err) {
    assert.ok(err);
  }
}

function testNonAligned() {
  var input = new Buffer(16);
  var output = new Buffer(16);
  var refoutput = new Buffer(16);
  var mask = 0xF0F0F0F0;

  for (var i = 0; i < input.length; ++i)
    input[i] = i;

  for (var end = 3; end > 0; --end) {
    referenceImplementation(input, mask, refoutput, 0, 0, end);
    input.mask(mask, output, 0, 0, end);

    for (var i = 0; i < end; ++i)
      assert.equal(output[i], refoutput[i]);
  }
}

function testReturnValue() {
  var input = new Buffer(16);
  var output = new Buffer(16);
  assert.equal(input.mask(0, output), 16)
  assert.equal(input.mask(0, output, 4), 12)
  assert.equal(input.mask(0, output, 4, 6), 10)
  assert.equal(input.mask(0, output, 4, 6, 4), 0)
  assert.equal(input.mask(0, output, 4, 6, 8), 2)
}
