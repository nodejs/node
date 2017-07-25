'use strict';
require('../common');
const assert = require('assert');

const b = Buffer.from('abcdef');
const buf_a = Buffer.from('a');
const buf_bc = Buffer.from('bc');
const buf_f = Buffer.from('f');
const buf_z = Buffer.from('z');
const buf_empty = Buffer.from('');

const s = 'abcdef';

assert.strictEqual(b.indexOf('a'), 0);
assert.strictEqual(b.indexOf('a', 1), -1);
assert.strictEqual(b.indexOf('a', -1), -1);
assert.strictEqual(b.indexOf('a', -4), -1);
assert.strictEqual(b.indexOf('a', -b.length), 0);
assert.strictEqual(b.indexOf('a', NaN), 0);
assert.strictEqual(b.indexOf('a', -Infinity), 0);
assert.strictEqual(b.indexOf('a', Infinity), -1);
assert.strictEqual(b.indexOf('bc'), 1);
assert.strictEqual(b.indexOf('bc', 2), -1);
assert.strictEqual(b.indexOf('bc', -1), -1);
assert.strictEqual(b.indexOf('bc', -3), -1);
assert.strictEqual(b.indexOf('bc', -5), 1);
assert.strictEqual(b.indexOf('bc', NaN), 1);
assert.strictEqual(b.indexOf('bc', -Infinity), 1);
assert.strictEqual(b.indexOf('bc', Infinity), -1);
assert.strictEqual(b.indexOf('f'), b.length - 1);
assert.strictEqual(b.indexOf('z'), -1);
assert.strictEqual(b.indexOf(''), 0);
assert.strictEqual(b.indexOf('', 1), 1);
assert.strictEqual(b.indexOf('', b.length + 1), b.length);
assert.strictEqual(b.indexOf('', Infinity), b.length);
assert.strictEqual(b.indexOf(buf_a), 0);
assert.strictEqual(b.indexOf(buf_a, 1), -1);
assert.strictEqual(b.indexOf(buf_a, -1), -1);
assert.strictEqual(b.indexOf(buf_a, -4), -1);
assert.strictEqual(b.indexOf(buf_a, -b.length), 0);
assert.strictEqual(b.indexOf(buf_a, NaN), 0);
assert.strictEqual(b.indexOf(buf_a, -Infinity), 0);
assert.strictEqual(b.indexOf(buf_a, Infinity), -1);
assert.strictEqual(b.indexOf(buf_bc), 1);
assert.strictEqual(b.indexOf(buf_bc, 2), -1);
assert.strictEqual(b.indexOf(buf_bc, -1), -1);
assert.strictEqual(b.indexOf(buf_bc, -3), -1);
assert.strictEqual(b.indexOf(buf_bc, -5), 1);
assert.strictEqual(b.indexOf(buf_bc, NaN), 1);
assert.strictEqual(b.indexOf(buf_bc, -Infinity), 1);
assert.strictEqual(b.indexOf(buf_bc, Infinity), -1);
assert.strictEqual(b.indexOf(buf_f), b.length - 1);
assert.strictEqual(b.indexOf(buf_z), -1);
assert.strictEqual(b.indexOf(buf_empty), 0);
assert.strictEqual(b.indexOf(buf_empty, 1), 1);
assert.strictEqual(b.indexOf(buf_empty, b.length + 1), b.length);
assert.strictEqual(b.indexOf(buf_empty, Infinity), b.length);
assert.strictEqual(b.indexOf(0x61), 0);
assert.strictEqual(b.indexOf(0x61, 1), -1);
assert.strictEqual(b.indexOf(0x61, -1), -1);
assert.strictEqual(b.indexOf(0x61, -4), -1);
assert.strictEqual(b.indexOf(0x61, -b.length), 0);
assert.strictEqual(b.indexOf(0x61, NaN), 0);
assert.strictEqual(b.indexOf(0x61, -Infinity), 0);
assert.strictEqual(b.indexOf(0x61, Infinity), -1);
assert.strictEqual(b.indexOf(0x0), -1);

// test offsets
assert.strictEqual(b.indexOf('d', 2), 3);
assert.strictEqual(b.indexOf('f', 5), 5);
assert.strictEqual(b.indexOf('f', -1), 5);
assert.strictEqual(b.indexOf('f', 6), -1);

assert.strictEqual(b.indexOf(Buffer.from('d'), 2), 3);
assert.strictEqual(b.indexOf(Buffer.from('f'), 5), 5);
assert.strictEqual(b.indexOf(Buffer.from('f'), -1), 5);
assert.strictEqual(b.indexOf(Buffer.from('f'), 6), -1);

assert.strictEqual(Buffer.from('ff').indexOf(Buffer.from('f'), 1, 'ucs2'), -1);

// test invalid and uppercase encoding
assert.strictEqual(b.indexOf('b', 'utf8'), 1);
assert.strictEqual(b.indexOf('b', 'UTF8'), 1);
assert.strictEqual(b.indexOf('62', 'HEX'), 1);
assert.throws(() => b.indexOf('bad', 'enc'), /Unknown encoding: enc/);

// test hex encoding
assert.strictEqual(
  Buffer.from(b.toString('hex'), 'hex')
    .indexOf('64', 0, 'hex'),
  3
);
assert.strictEqual(
  Buffer.from(b.toString('hex'), 'hex')
    .indexOf(Buffer.from('64', 'hex'), 0, 'hex'),
  3
);

// test base64 encoding
assert.strictEqual(
  Buffer.from(b.toString('base64'), 'base64')
    .indexOf('ZA==', 0, 'base64'),
  3
);
assert.strictEqual(
  Buffer.from(b.toString('base64'), 'base64')
    .indexOf(Buffer.from('ZA==', 'base64'), 0, 'base64'),
  3
);

// test ascii encoding
assert.strictEqual(
  Buffer.from(b.toString('ascii'), 'ascii')
    .indexOf('d', 0, 'ascii'),
  3
);
assert.strictEqual(
  Buffer.from(b.toString('ascii'), 'ascii')
    .indexOf(Buffer.from('d', 'ascii'), 0, 'ascii'),
  3
);

// test latin1 encoding
assert.strictEqual(
  Buffer.from(b.toString('latin1'), 'latin1')
    .indexOf('d', 0, 'latin1'),
  3
);
assert.strictEqual(
  Buffer.from(b.toString('latin1'), 'latin1')
    .indexOf(Buffer.from('d', 'latin1'), 0, 'latin1'),
  3
);
assert.strictEqual(
  Buffer.from('aa\u00e8aa', 'latin1')
    .indexOf('\u00e8', 'latin1'),
  2
);
assert.strictEqual(
  Buffer.from('\u00e8', 'latin1')
    .indexOf('\u00e8', 'latin1'),
  0
);
assert.strictEqual(
  Buffer.from('\u00e8', 'latin1')
    .indexOf(Buffer.from('\u00e8', 'latin1'), 'latin1'),
  0
);

// test binary encoding
assert.strictEqual(
  Buffer.from(b.toString('binary'), 'binary')
    .indexOf('d', 0, 'binary'),
  3
);
assert.strictEqual(
  Buffer.from(b.toString('binary'), 'binary')
    .indexOf(Buffer.from('d', 'binary'), 0, 'binary'),
  3
);
assert.strictEqual(
  Buffer.from('aa\u00e8aa', 'binary')
    .indexOf('\u00e8', 'binary'),
  2
);
assert.strictEqual(
  Buffer.from('\u00e8', 'binary')
    .indexOf('\u00e8', 'binary'),
  0
);
assert.strictEqual(
  Buffer.from('\u00e8', 'binary')
    .indexOf(Buffer.from('\u00e8', 'binary'), 'binary'),
  0
);


// test optional offset with passed encoding
assert.strictEqual(Buffer.from('aaaa0').indexOf('30', 'hex'), 4);
assert.strictEqual(Buffer.from('aaaa00a').indexOf('3030', 'hex'), 4);

{
  // test usc2 encoding
  const twoByteString = Buffer.from('\u039a\u0391\u03a3\u03a3\u0395', 'ucs2');

  assert.strictEqual(8, twoByteString.indexOf('\u0395', 4, 'ucs2'));
  assert.strictEqual(6, twoByteString.indexOf('\u03a3', -4, 'ucs2'));
  assert.strictEqual(4, twoByteString.indexOf('\u03a3', -6, 'ucs2'));
  assert.strictEqual(4, twoByteString.indexOf(
    Buffer.from('\u03a3', 'ucs2'), -6, 'ucs2'));
  assert.strictEqual(-1, twoByteString.indexOf('\u03a3', -2, 'ucs2'));
}

const mixedByteStringUcs2 =
    Buffer.from('\u039a\u0391abc\u03a3\u03a3\u0395', 'ucs2');
assert.strictEqual(6, mixedByteStringUcs2.indexOf('bc', 0, 'ucs2'));
assert.strictEqual(10, mixedByteStringUcs2.indexOf('\u03a3', 0, 'ucs2'));
assert.strictEqual(-1, mixedByteStringUcs2.indexOf('\u0396', 0, 'ucs2'));

assert.strictEqual(
  6, mixedByteStringUcs2.indexOf(Buffer.from('bc', 'ucs2'), 0, 'ucs2'));
assert.strictEqual(
  10, mixedByteStringUcs2.indexOf(Buffer.from('\u03a3', 'ucs2'), 0, 'ucs2'));
assert.strictEqual(
  -1, mixedByteStringUcs2.indexOf(Buffer.from('\u0396', 'ucs2'), 0, 'ucs2'));

{
  const twoByteString = Buffer.from('\u039a\u0391\u03a3\u03a3\u0395', 'ucs2');

  // Test single char pattern
  assert.strictEqual(0, twoByteString.indexOf('\u039a', 0, 'ucs2'));
  assert.strictEqual(2, twoByteString.indexOf('\u0391', 0, 'ucs2'),
                     'Alpha');
  assert.strictEqual(4, twoByteString.indexOf('\u03a3', 0, 'ucs2'),
                     'First Sigma');
  assert.strictEqual(6, twoByteString.indexOf('\u03a3', 6, 'ucs2'),
                     'Second Sigma');
  assert.strictEqual(8, twoByteString.indexOf('\u0395', 0, 'ucs2'),
                     'Epsilon');
  assert.strictEqual(-1, twoByteString.indexOf('\u0392', 0, 'ucs2'),
                     'Not beta');

  // Test multi-char pattern
  assert.strictEqual(
    0, twoByteString.indexOf('\u039a\u0391', 0, 'ucs2'), 'Lambda Alpha');
  assert.strictEqual(
    2, twoByteString.indexOf('\u0391\u03a3', 0, 'ucs2'), 'Alpha Sigma');
  assert.strictEqual(
    4, twoByteString.indexOf('\u03a3\u03a3', 0, 'ucs2'), 'Sigma Sigma');
  assert.strictEqual(
    6, twoByteString.indexOf('\u03a3\u0395', 0, 'ucs2'), 'Sigma Epsilon');
}

const mixedByteStringUtf8 = Buffer.from('\u039a\u0391abc\u03a3\u03a3\u0395');
assert.strictEqual(5, mixedByteStringUtf8.indexOf('bc'));
assert.strictEqual(5, mixedByteStringUtf8.indexOf('bc', 5));
assert.strictEqual(5, mixedByteStringUtf8.indexOf('bc', -8));
assert.strictEqual(7, mixedByteStringUtf8.indexOf('\u03a3'));
assert.strictEqual(-1, mixedByteStringUtf8.indexOf('\u0396'));


// Test complex string indexOf algorithms. Only trigger for long strings.
// Long string that isn't a simple repeat of a shorter string.
let longString = 'A';
for (let i = 66; i < 76; i++) {  // from 'B' to 'K'
  longString = longString + String.fromCharCode(i) + longString;
}

const longBufferString = Buffer.from(longString);

// pattern of 15 chars, repeated every 16 chars in long
let pattern = 'ABACABADABACABA';
for (let i = 0; i < longBufferString.length - pattern.length; i += 7) {
  const index = longBufferString.indexOf(pattern, i);
  assert.strictEqual((i + 15) & ~0xf, index,
                     `Long ABACABA...-string at index ${i}`);
}
assert.strictEqual(510, longBufferString.indexOf('AJABACA'),
                   'Long AJABACA, First J');
assert.strictEqual(
  1534, longBufferString.indexOf('AJABACA', 511), 'Long AJABACA, Second J');

pattern = 'JABACABADABACABA';
assert.strictEqual(
  511, longBufferString.indexOf(pattern), 'Long JABACABA..., First J');
assert.strictEqual(
  1535, longBufferString.indexOf(pattern, 512), 'Long JABACABA..., Second J');

// Search for a non-ASCII string in a pure ASCII string.
const asciiString = Buffer.from(
  'arglebargleglopglyfarglebargleglopglyfarglebargleglopglyf');
assert.strictEqual(-1, asciiString.indexOf('\x2061'));
assert.strictEqual(3, asciiString.indexOf('leb', 0));

// Search in string containing many non-ASCII chars.
const allCodePoints = [];
for (let i = 0; i < 65536; i++) allCodePoints[i] = i;
const allCharsString = String.fromCharCode.apply(String, allCodePoints);
const allCharsBufferUtf8 = Buffer.from(allCharsString);
const allCharsBufferUcs2 = Buffer.from(allCharsString, 'ucs2');

// Search for string long enough to trigger complex search with ASCII pattern
// and UC16 subject.
assert.strictEqual(-1, allCharsBufferUtf8.indexOf('notfound'));
assert.strictEqual(-1, allCharsBufferUcs2.indexOf('notfound'));

// Needle is longer than haystack, but only because it's encoded as UTF-16
assert.strictEqual(Buffer.from('aaaa').indexOf('a'.repeat(4), 'ucs2'), -1);

assert.strictEqual(Buffer.from('aaaa').indexOf('a'.repeat(4), 'utf8'), 0);
assert.strictEqual(Buffer.from('aaaa').indexOf('你好', 'ucs2'), -1);

// Haystack has odd length, but the needle is UCS2.
assert.strictEqual(Buffer.from('aaaaa').indexOf('b', 'ucs2'), -1);

{
  // Find substrings in Utf8.
  const lengths = [1, 3, 15];  // Single char, simple and complex.
  const indices = [0x5, 0x60, 0x400, 0x680, 0x7ee, 0xFF02, 0x16610, 0x2f77b];
  for (let lengthIndex = 0; lengthIndex < lengths.length; lengthIndex++) {
    for (let i = 0; i < indices.length; i++) {
      const index = indices[i];
      let length = lengths[lengthIndex];

      if (index + length > 0x7F) {
        length = 2 * length;
      }

      if (index + length > 0x7FF) {
        length = 3 * length;
      }

      if (index + length > 0xFFFF) {
        length = 4 * length;
      }

      const patternBufferUtf8 = allCharsBufferUtf8.slice(index, index + length);
      assert.strictEqual(index, allCharsBufferUtf8.indexOf(patternBufferUtf8));

      const patternStringUtf8 = patternBufferUtf8.toString();
      assert.strictEqual(index, allCharsBufferUtf8.indexOf(patternStringUtf8));
    }
  }
}

{
  // Find substrings in Usc2.
  const lengths = [2, 4, 16];  // Single char, simple and complex.
  const indices = [0x5, 0x65, 0x105, 0x205, 0x285, 0x2005, 0x2085, 0xfff0];
  for (let lengthIndex = 0; lengthIndex < lengths.length; lengthIndex++) {
    for (let i = 0; i < indices.length; i++) {
      const index = indices[i] * 2;
      const length = lengths[lengthIndex];

      const patternBufferUcs2 =
          allCharsBufferUcs2.slice(index, index + length);
      assert.strictEqual(
        index, allCharsBufferUcs2.indexOf(patternBufferUcs2, 0, 'ucs2'));

      const patternStringUcs2 = patternBufferUcs2.toString('ucs2');
      assert.strictEqual(
        index, allCharsBufferUcs2.indexOf(patternStringUcs2, 0, 'ucs2'));
    }
  }
}

const argumentExpected =
    /^TypeError: "val" argument must be string, number, Buffer or Uint8Array$/;

assert.throws(() => {
  b.indexOf(() => { });
}, argumentExpected);

assert.throws(() => {
  b.indexOf({});
}, argumentExpected);

assert.throws(() => {
  b.indexOf([]);
}, argumentExpected);

// Test weird offset arguments.
// The following offsets coerce to NaN or 0, searching the whole Buffer
assert.strictEqual(b.indexOf('b', undefined), 1);
assert.strictEqual(b.indexOf('b', {}), 1);
assert.strictEqual(b.indexOf('b', 0), 1);
assert.strictEqual(b.indexOf('b', null), 1);
assert.strictEqual(b.indexOf('b', []), 1);

// The following offset coerces to 2, in other words +[2] === 2
assert.strictEqual(b.indexOf('b', [2]), -1);

// Behavior should match String.indexOf()
assert.strictEqual(
  b.indexOf('b', undefined),
  s.indexOf('b', undefined));
assert.strictEqual(
  b.indexOf('b', {}),
  s.indexOf('b', {}));
assert.strictEqual(
  b.indexOf('b', 0),
  s.indexOf('b', 0));
assert.strictEqual(
  b.indexOf('b', null),
  s.indexOf('b', null));
assert.strictEqual(
  b.indexOf('b', []),
  s.indexOf('b', []));
assert.strictEqual(
  b.indexOf('b', [2]),
  s.indexOf('b', [2]));

// All code for handling encodings is shared between Buffer.indexOf and
// Buffer.lastIndexOf, so only testing the separate lastIndexOf semantics.

// Test lastIndexOf basic functionality; Buffer b contains 'abcdef'.
// lastIndexOf string:
assert.strictEqual(b.lastIndexOf('a'), 0);
assert.strictEqual(b.lastIndexOf('a', 1), 0);
assert.strictEqual(b.lastIndexOf('b', 1), 1);
assert.strictEqual(b.lastIndexOf('c', 1), -1);
assert.strictEqual(b.lastIndexOf('a', -1), 0);
assert.strictEqual(b.lastIndexOf('a', -4), 0);
assert.strictEqual(b.lastIndexOf('a', -b.length), 0);
assert.strictEqual(b.lastIndexOf('a', -b.length - 1), -1);
assert.strictEqual(b.lastIndexOf('a', NaN), 0);
assert.strictEqual(b.lastIndexOf('a', -Infinity), -1);
assert.strictEqual(b.lastIndexOf('a', Infinity), 0);
// lastIndexOf Buffer:
assert.strictEqual(b.lastIndexOf(buf_a), 0);
assert.strictEqual(b.lastIndexOf(buf_a, 1), 0);
assert.strictEqual(b.lastIndexOf(buf_a, -1), 0);
assert.strictEqual(b.lastIndexOf(buf_a, -4), 0);
assert.strictEqual(b.lastIndexOf(buf_a, -b.length), 0);
assert.strictEqual(b.lastIndexOf(buf_a, -b.length - 1), -1);
assert.strictEqual(b.lastIndexOf(buf_a, NaN), 0);
assert.strictEqual(b.lastIndexOf(buf_a, -Infinity), -1);
assert.strictEqual(b.lastIndexOf(buf_a, Infinity), 0);
assert.strictEqual(b.lastIndexOf(buf_bc), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, 2), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, -1), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, -3), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, -5), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, -6), -1);
assert.strictEqual(b.lastIndexOf(buf_bc, NaN), 1);
assert.strictEqual(b.lastIndexOf(buf_bc, -Infinity), -1);
assert.strictEqual(b.lastIndexOf(buf_bc, Infinity), 1);
assert.strictEqual(b.lastIndexOf(buf_f), b.length - 1);
assert.strictEqual(b.lastIndexOf(buf_z), -1);
assert.strictEqual(b.lastIndexOf(buf_empty), b.length);
assert.strictEqual(b.lastIndexOf(buf_empty, 1), 1);
assert.strictEqual(b.lastIndexOf(buf_empty, b.length + 1), b.length);
assert.strictEqual(b.lastIndexOf(buf_empty, Infinity), b.length);
// lastIndexOf number:
assert.strictEqual(b.lastIndexOf(0x61), 0);
assert.strictEqual(b.lastIndexOf(0x61, 1), 0);
assert.strictEqual(b.lastIndexOf(0x61, -1), 0);
assert.strictEqual(b.lastIndexOf(0x61, -4), 0);
assert.strictEqual(b.lastIndexOf(0x61, -b.length), 0);
assert.strictEqual(b.lastIndexOf(0x61, -b.length - 1), -1);
assert.strictEqual(b.lastIndexOf(0x61, NaN), 0);
assert.strictEqual(b.lastIndexOf(0x61, -Infinity), -1);
assert.strictEqual(b.lastIndexOf(0x61, Infinity), 0);
assert.strictEqual(b.lastIndexOf(0x0), -1);

// Test weird offset arguments.
// The following offsets coerce to NaN, searching the whole Buffer
assert.strictEqual(b.lastIndexOf('b', undefined), 1);
assert.strictEqual(b.lastIndexOf('b', {}), 1);

// The following offsets coerce to 0
assert.strictEqual(b.lastIndexOf('b', 0), -1);
assert.strictEqual(b.lastIndexOf('b', null), -1);
assert.strictEqual(b.lastIndexOf('b', []), -1);

// The following offset coerces to 2, in other words +[2] === 2
assert.strictEqual(b.lastIndexOf('b', [2]), 1);

// Behavior should match String.lastIndexOf()
assert.strictEqual(
  b.lastIndexOf('b', undefined),
  s.lastIndexOf('b', undefined));
assert.strictEqual(
  b.lastIndexOf('b', {}),
  s.lastIndexOf('b', {}));
assert.strictEqual(
  b.lastIndexOf('b', 0),
  s.lastIndexOf('b', 0));
assert.strictEqual(
  b.lastIndexOf('b', null),
  s.lastIndexOf('b', null));
assert.strictEqual(
  b.lastIndexOf('b', []),
  s.lastIndexOf('b', []));
assert.strictEqual(
  b.lastIndexOf('b', [2]),
  s.lastIndexOf('b', [2]));

// Test needles longer than the haystack.
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 'ucs2'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 'utf8'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 'latin1'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 'binary'), -1);
assert.strictEqual(b.lastIndexOf(Buffer.from('aaaaaaaaaaaaaaa')), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 2, 'ucs2'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 3, 'utf8'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 5, 'latin1'), -1);
assert.strictEqual(b.lastIndexOf('aaaaaaaaaaaaaaa', 5, 'binary'), -1);
assert.strictEqual(b.lastIndexOf(Buffer.from('aaaaaaaaaaaaaaa'), 7), -1);

// 你好 expands to a total of 6 bytes using UTF-8 and 4 bytes using UTF-16
assert.strictEqual(buf_bc.lastIndexOf('你好', 'ucs2'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 'utf8'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 'latin1'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 'binary'), -1);
assert.strictEqual(buf_bc.lastIndexOf(Buffer.from('你好')), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 2, 'ucs2'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 3, 'utf8'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 5, 'latin1'), -1);
assert.strictEqual(buf_bc.lastIndexOf('你好', 5, 'binary'), -1);
assert.strictEqual(buf_bc.lastIndexOf(Buffer.from('你好'), 7), -1);

// Test lastIndexOf on a longer buffer:
const bufferString = new Buffer('a man a plan a canal panama');
assert.strictEqual(15, bufferString.lastIndexOf('canal'));
assert.strictEqual(21, bufferString.lastIndexOf('panama'));
assert.strictEqual(0, bufferString.lastIndexOf('a man a plan a canal panama'));
assert.strictEqual(-1, bufferString.lastIndexOf('a man a plan a canal mexico'));
assert.strictEqual(-1, bufferString
  .lastIndexOf('a man a plan a canal mexico city'));
assert.strictEqual(-1, bufferString.lastIndexOf(Buffer.from('a'.repeat(1000))));
assert.strictEqual(0, bufferString.lastIndexOf('a man a plan', 4));
assert.strictEqual(13, bufferString.lastIndexOf('a '));
assert.strictEqual(13, bufferString.lastIndexOf('a ', 13));
assert.strictEqual(6, bufferString.lastIndexOf('a ', 12));
assert.strictEqual(0, bufferString.lastIndexOf('a ', 5));
assert.strictEqual(13, bufferString.lastIndexOf('a ', -1));
assert.strictEqual(0, bufferString.lastIndexOf('a ', -27));
assert.strictEqual(-1, bufferString.lastIndexOf('a ', -28));

// Test lastIndexOf for the case that the first character can be found,
// but in a part of the buffer that does not make search to search
// due do length constraints.
const abInUCS2 = Buffer.from('ab', 'ucs2');
assert.strictEqual(-1, Buffer.from('µaaaa¶bbbb', 'latin1').lastIndexOf('µ'));
assert.strictEqual(-1, Buffer.from('µaaaa¶bbbb', 'binary').lastIndexOf('µ'));
assert.strictEqual(-1, Buffer.from('bc').lastIndexOf('ab'));
assert.strictEqual(-1, Buffer.from('abc').lastIndexOf('qa'));
assert.strictEqual(-1, Buffer.from('abcdef').lastIndexOf('qabc'));
assert.strictEqual(-1, Buffer.from('bc').lastIndexOf(Buffer.from('ab')));
assert.strictEqual(-1, Buffer.from('bc', 'ucs2').lastIndexOf('ab', 'ucs2'));
assert.strictEqual(-1, Buffer.from('bc', 'ucs2').lastIndexOf(abInUCS2));

assert.strictEqual(0, Buffer.from('abc').lastIndexOf('ab'));
assert.strictEqual(0, Buffer.from('abc').lastIndexOf('ab', 1));
assert.strictEqual(0, Buffer.from('abc').lastIndexOf('ab', 2));
assert.strictEqual(0, Buffer.from('abc').lastIndexOf('ab', 3));

// The above tests test the LINEAR and SINGLE-CHAR strategies.
// Now, we test the BOYER-MOORE-HORSPOOL strategy.
// Test lastIndexOf on a long buffer w multiple matches:
pattern = 'JABACABADABACABA';
assert.strictEqual(1535, longBufferString.lastIndexOf(pattern));
assert.strictEqual(1535, longBufferString.lastIndexOf(pattern, 1535));
assert.strictEqual(511, longBufferString.lastIndexOf(pattern, 1534));

// Finally, give it a really long input to trigger fallback from BMH to
// regular BOYER-MOORE (which has better worst-case complexity).

// Generate a really long Thue-Morse sequence of 'yolo' and 'swag',
// "yolo swag swag yolo swag yolo yolo swag" ..., goes on for about 5MB.
// This is hard to search because it all looks similar, but never repeats.

// countBits returns the number of bits in the binary reprsentation of n.
function countBits(n) {
  let count;
  for (count = 0; n > 0; count++) {
    n = n & (n - 1); // remove top bit
  }
  return count;
}
const parts = [];
for (let i = 0; i < 1000000; i++) {
  parts.push((countBits(i) % 2 === 0) ? 'yolo' : 'swag');
}
const reallyLong = new Buffer(parts.join(' '));
assert.strictEqual('yolo swag swag yolo', reallyLong.slice(0, 19).toString());

// Expensive reverse searches. Stress test lastIndexOf:
pattern = reallyLong.slice(0, 100000);  // First 1/50th of the pattern.
assert.strictEqual(4751360, reallyLong.lastIndexOf(pattern));
assert.strictEqual(3932160, reallyLong.lastIndexOf(pattern, 4000000));
assert.strictEqual(2949120, reallyLong.lastIndexOf(pattern, 3000000));
pattern = reallyLong.slice(100000, 200000);  // Second 1/50th.
assert.strictEqual(4728480, reallyLong.lastIndexOf(pattern));
pattern = reallyLong.slice(0, 1000000);  // First 1/5th.
assert.strictEqual(3932160, reallyLong.lastIndexOf(pattern));
pattern = reallyLong.slice(0, 2000000);  // first 2/5ths.
assert.strictEqual(0, reallyLong.lastIndexOf(pattern));

// test truncation of Number arguments to uint8
{
  const buf = Buffer.from('this is a test');
  assert.strictEqual(buf.indexOf(0x6973), 3);
  assert.strictEqual(buf.indexOf(0x697320), 4);
  assert.strictEqual(buf.indexOf(0x69732069), 2);
  assert.strictEqual(buf.indexOf(0x697374657374), 0);
  assert.strictEqual(buf.indexOf(0x69737374), 0);
  assert.strictEqual(buf.indexOf(0x69737465), 11);
  assert.strictEqual(buf.indexOf(0x69737465), 11);
  assert.strictEqual(buf.indexOf(-140), 0);
  assert.strictEqual(buf.indexOf(-152), 1);
  assert.strictEqual(buf.indexOf(0xff), -1);
  assert.strictEqual(buf.indexOf(0xffff), -1);
}

// Test that Uint8Array arguments are okay.
{
  const needle = new Uint8Array([ 0x66, 0x6f, 0x6f ]);
  const haystack = Buffer.from('a foo b foo');
  assert.strictEqual(haystack.indexOf(needle), 2);
  assert.strictEqual(haystack.lastIndexOf(needle), haystack.length - 3);
}
