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

'use strict';
const common = require('../common');
const assert = require('assert');
const inspect = require('util').inspect;
const StringDecoder = require('string_decoder').StringDecoder;

// Test default encoding
let decoder = new StringDecoder();
assert.strictEqual(decoder.encoding, 'utf8');

// Should work without 'new' keyword
const decoder2 = {};
StringDecoder.call(decoder2);
assert.strictEqual(decoder2.encoding, 'utf8');

// UTF-8
test('utf-8', Buffer.from('$', 'utf-8'), '$');
test('utf-8', Buffer.from('¢', 'utf-8'), '¢');
test('utf-8', Buffer.from('€', 'utf-8'), '€');
test('utf-8', Buffer.from('𤭢', 'utf-8'), '𤭢');
// A mixed ascii and non-ascii string
// Test stolen from deps/v8/test/cctest/test-strings.cc
// U+02E4 -> CB A4
// U+0064 -> 64
// U+12E4 -> E1 8B A4
// U+0030 -> 30
// U+3045 -> E3 81 85
test(
  'utf-8',
  Buffer.from([0xCB, 0xA4, 0x64, 0xE1, 0x8B, 0xA4, 0x30, 0xE3, 0x81, 0x85]),
  '\u02e4\u0064\u12e4\u0030\u3045'
);

// Some invalid input, known to have caused trouble with chunking
// in https://github.com/nodejs/node/pull/7310#issuecomment-226445923
// 00: |00000000 ASCII
// 41: |01000001 ASCII
// B8: 10|111000 continuation
// CC: 110|01100 two-byte head
// E2: 1110|0010 three-byte head
// F0: 11110|000 four-byte head
// F1: 11110|001'another four-byte head
// FB: 111110|11 "five-byte head", not UTF-8
test('utf-8', Buffer.from('C9B5A941', 'hex'), '\u0275\ufffdA');
test('utf-8', Buffer.from('E2', 'hex'), '\ufffd');
test('utf-8', Buffer.from('E241', 'hex'), '\ufffdA');
test('utf-8', Buffer.from('CCCCB8', 'hex'), '\ufffd\u0338');
test('utf-8', Buffer.from('F0B841', 'hex'), '\ufffdA');
test('utf-8', Buffer.from('F1CCB8', 'hex'), '\ufffd\u0338');
test('utf-8', Buffer.from('F0FB00', 'hex'), '\ufffd\ufffd\0');
test('utf-8', Buffer.from('CCE2B8B8', 'hex'), '\ufffd\u2e38');
test('utf-8', Buffer.from('E2B8CCB8', 'hex'), '\ufffd\u0338');
test('utf-8', Buffer.from('E2FBCC01', 'hex'), '\ufffd\ufffd\ufffd\u0001');
test('utf-8', Buffer.from('CCB8CDB9', 'hex'), '\u0338\u0379');
// CESU-8 of U+1D40D

// V8 has changed their invalid UTF-8 handling, see
// https://chromium-review.googlesource.com/c/v8/v8/+/671020 for more info.
test('utf-8', Buffer.from('EDA0B5EDB08D', 'hex'),
     '\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd');

// UCS-2
test('ucs2', Buffer.from('ababc', 'ucs2'), 'ababc');

// UTF-16LE
test('utf16le', Buffer.from('3DD84DDC', 'hex'), '\ud83d\udc4d'); // thumbs up

// Additional UTF-8 tests
decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('E1', 'hex')), '');

// A quick test for lastChar, lastNeed & lastTotal which are undocumented.
assert(decoder.lastChar.equals(new Uint8Array([0xe1, 0, 0, 0])));
assert.strictEqual(decoder.lastNeed, 2);
assert.strictEqual(decoder.lastTotal, 3);

assert.strictEqual(decoder.end(), '\ufffd');

// ArrayBufferView tests
const arrayBufferViewStr = 'String for ArrayBufferView tests\n';
const inputBuffer = Buffer.from(arrayBufferViewStr.repeat(8), 'utf8');
for (const expectView of common.getArrayBufferViews(inputBuffer)) {
  assert.strictEqual(
    decoder.write(expectView),
    inputBuffer.toString('utf8')
  );
  assert.strictEqual(decoder.end(), '');
}

decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('E18B', 'hex')), '');
assert.strictEqual(decoder.end(), '\ufffd');

decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('\ufffd')), '\ufffd');
assert.strictEqual(decoder.end(), '');

decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('\ufffd\ufffd\ufffd')),
                   '\ufffd\ufffd\ufffd');
assert.strictEqual(decoder.end(), '');

decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('EFBFBDE2', 'hex')), '\ufffd');
assert.strictEqual(decoder.end(), '\ufffd');

decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('F1', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('41F2', 'hex')), '\ufffdA');
assert.strictEqual(decoder.end(), '\ufffd');

// Additional utf8Text test
decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.text(Buffer.from([0x41]), 2), '');

// Additional UTF-16LE surrogate pair tests
decoder = new StringDecoder('utf16le');
assert.strictEqual(decoder.write(Buffer.from('3DD8', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('4D', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('DC', 'hex')), '\ud83d\udc4d');
assert.strictEqual(decoder.end(), '');

decoder = new StringDecoder('utf16le');
assert.strictEqual(decoder.write(Buffer.from('3DD8', 'hex')), '');
assert.strictEqual(decoder.end(), '\ud83d');

decoder = new StringDecoder('utf16le');
assert.strictEqual(decoder.write(Buffer.from('3DD8', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('4D', 'hex')), '');
assert.strictEqual(decoder.end(), '\ud83d');

decoder = new StringDecoder('utf16le');
assert.strictEqual(decoder.write(Buffer.from('3DD84D', 'hex')), '\ud83d');
assert.strictEqual(decoder.end(), '');

// Regression test for https://github.com/nodejs/node/issues/22358
// (unaligned UTF-16 access).
decoder = new StringDecoder('utf16le');
assert.strictEqual(decoder.write(Buffer.alloc(1)), '');
assert.strictEqual(decoder.write(Buffer.alloc(20)), '\0'.repeat(10));
assert.strictEqual(decoder.write(Buffer.alloc(48)), '\0'.repeat(24));
assert.strictEqual(decoder.end(), '');

// Regression tests for https://github.com/nodejs/node/issues/22626
// (not enough replacement chars when having seen more than one byte of an
// incomplete multibyte characters).
decoder = new StringDecoder('utf8');
assert.strictEqual(decoder.write(Buffer.from('f69b', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('d1', 'hex')), '\ufffd\ufffd');
assert.strictEqual(decoder.end(), '\ufffd');
assert.strictEqual(decoder.write(Buffer.from('f4', 'hex')), '');
assert.strictEqual(decoder.write(Buffer.from('bde5', 'hex')), '\ufffd\ufffd');
assert.strictEqual(decoder.end(), '\ufffd');

assert.throws(
  () => new StringDecoder(1),
  {
    code: 'ERR_UNKNOWN_ENCODING',
    name: 'TypeError',
    message: 'Unknown encoding: 1'
  }
);

assert.throws(
  () => new StringDecoder('test'),
  {
    code: 'ERR_UNKNOWN_ENCODING',
    name: 'TypeError',
    message: 'Unknown encoding: test'
  }
);

assert.throws(
  () => new StringDecoder('utf8').write(null),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "buf" argument must be an instance of Buffer, TypedArray,' +
      ' or DataView. Received null'
  }
);

// Test verifies that StringDecoder will correctly decode the given input
// buffer with the given encoding to the expected output. It will attempt all
// possible ways to write() the input buffer, see writeSequences(). The
// singleSequence allows for easy debugging of a specific sequence which is
// useful in case of test failures.
function test(encoding, input, expected, singleSequence) {
  let sequences;
  if (!singleSequence) {
    sequences = writeSequences(input.length);
  } else {
    sequences = [singleSequence];
  }
  const hexNumberRE = /.{2}/g;
  sequences.forEach((sequence) => {
    const decoder = new StringDecoder(encoding);
    let output = '';
    sequence.forEach((write) => {
      output += decoder.write(input.slice(write[0], write[1]));
    });
    output += decoder.end();
    if (output !== expected) {
      const message =
        `Expected "${unicodeEscape(expected)}", ` +
        `but got "${unicodeEscape(output)}"\n` +
        `input: ${input.toString('hex').match(hexNumberRE)}\n` +
        `Write sequence: ${JSON.stringify(sequence)}\n` +
        `Full Decoder State: ${inspect(decoder)}`;
      assert.fail(message);
    }
  });
}

// unicodeEscape prints the str contents as unicode escape codes.
function unicodeEscape(str) {
  let r = '';
  for (let i = 0; i < str.length; i++) {
    r += `\\u${str.charCodeAt(i).toString(16)}`;
  }
  return r;
}

// writeSequences returns an array of arrays that describes all possible ways a
// buffer of the given length could be split up and passed to sequential write
// calls.
//
// e.G. writeSequences(3) will return: [
//   [ [ 0, 3 ] ],
//   [ [ 0, 2 ], [ 2, 3 ] ],
//   [ [ 0, 1 ], [ 1, 3 ] ],
//   [ [ 0, 1 ], [ 1, 2 ], [ 2, 3 ] ]
// ]
function writeSequences(length, start, sequence) {
  if (start === undefined) {
    start = 0;
    sequence = [];
  } else if (start === length) {
    return [sequence];
  }
  let sequences = [];
  for (let end = length; end > start; end--) {
    const subSequence = sequence.concat([[start, end]]);
    const subSequences = writeSequences(length, end, subSequence, sequences);
    sequences = sequences.concat(subSequences);
  }
  return sequences;
}
