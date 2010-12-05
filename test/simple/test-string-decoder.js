var common = require('../common');
var assert = require('assert');
var StringDecoder = require('string_decoder').StringDecoder;
var decoder = new StringDecoder('utf8');



var buffer = new Buffer('$');
assert.deepEqual('$', decoder.write(buffer));

buffer = new Buffer('¢');
assert.deepEqual('', decoder.write(buffer.slice(0, 1)));
assert.deepEqual('¢', decoder.write(buffer.slice(1, 2)));

buffer = new Buffer('€');
assert.deepEqual('', decoder.write(buffer.slice(0, 1)));
assert.deepEqual('', decoder.write(buffer.slice(1, 2)));
assert.deepEqual('€', decoder.write(buffer.slice(2, 3)));

buffer = new Buffer([0xF0, 0xA4, 0xAD, 0xA2]);
var s = '';
s += decoder.write(buffer.slice(0, 1));
s += decoder.write(buffer.slice(1, 2));
s += decoder.write(buffer.slice(2, 3));
s += decoder.write(buffer.slice(3, 4));
assert.ok(s.length > 0);

// A mixed ascii and non-ascii string
// Test stolen from deps/v8/test/cctest/test-strings.cc
// U+02E4 -> CB A4
// U+0064 -> 64
// U+12E4 -> E1 8B A4
// U+0030 -> 30
// U+3045 -> E3 81 85
var expected = '\u02e4\u0064\u12e4\u0030\u3045';
var buffer = new Buffer([0xCB, 0xA4, 0x64, 0xE1, 0x8B, 0xA4,
                         0x30, 0xE3, 0x81, 0x85]);
var charLengths = [0, 0, 1, 2, 2, 2, 3, 4, 4, 4, 5, 5];

// Split the buffer into 3 segments
//  |----|------|-------|
//  0    i      j       buffer.length
// Scan through every possible 3 segment combination
// and make sure that the string is always parsed.
common.print('scanning ');
for (var j = 2; j < buffer.length; j++) {
  for (var i = 1; i < j; i++) {
    var decoder = new StringDecoder('utf8');

    var sum = decoder.write(buffer.slice(0, i));

    // just check that we've received the right amount
    // after the first write
    assert.equal(charLengths[i], sum.length);

    sum += decoder.write(buffer.slice(i, j));
    sum += decoder.write(buffer.slice(j, buffer.length));
    assert.equal(expected, sum);
    common.print('.');
  }
}
console.log(' crayon!');

