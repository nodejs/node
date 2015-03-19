var common = require('../common');
var assert = require('assert');

var tests = [
  testBasic,
  testBounds,
  testOverflow,
  testNonAligned
]

tests.forEach(Function.prototype.call.bind(Function.prototype.call))

function referenceImplementation(source, maskNum, output, offset, length) {
  var i = 0;
  var mask = new Buffer(4);
  mask.writeUInt32LE(maskNum, 0);

  for (; i < length - 3; i += 4) {
    var num = (maskNum ^ source.readUInt32LE(i, true)) >>> 0;
    if (num < 0) num = 4294967296 + num;
    output.writeUInt32LE(num, offset + i, true);
  }

  switch (length % 4) {
    case 3: output[offset + i + 2] = source[i + 2] ^ mask[2];
    case 2: output[offset + i + 1] = source[i + 1] ^ mask[1];
    case 1: output[offset + i] = source[i] ^ mask[0];
  }
}

function testBasic() {
  var input = new Buffer(256);
  var output = new Buffer(256);
  var refoutput = new Buffer(256);
  var mask = 0xF0F0F0F0;

  for (var i = 0; i < input.length; ++i)
    input[i] = i;

  referenceImplementation(input, mask, refoutput, 0, 256);
  input.mask(mask, output, 256);

  for(var i = 0; i < input.length; ++i)
    assert.equal(output[i], refoutput[i]);
}

function testBounds() {
  var input = new Buffer(16)
  var output = new Buffer(32)
  try {
    input.mask(120120, output, 17)
    assert.fail('expected error')
  } catch(err) {
    assert.ok(err)
  }
}

function testOverflow() {
  var input = new Buffer(16)
  var output = new Buffer(15)
  try {
    input.mask(120120, output)
    assert.fail('expected error')
  } catch(err) {
    assert.ok(err)
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
    referenceImplementation(input, mask, refoutput, 0, end);
    input.mask(mask, output, end);

    for(var i = 0; i < end; ++i)
      assert.equal(output[i], refoutput[i]);
  }
}
