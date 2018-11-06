// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { codes: { ERR_OUT_OF_RANGE } } = require('internal/errors');
const { internalBinding } = require('internal/test/binding');
const SIZE = 28;

const buf1 = Buffer.allocUnsafe(SIZE);
const buf2 = Buffer.allocUnsafe(SIZE);

// Default encoding
testBufs('abc');
testBufs('\u0222aa');
testBufs('a\u0234b\u0235c\u0236');
testBufs('abc', 4);
testBufs('abc', 5);
testBufs('abc', SIZE);
testBufs('\u0222aa', 2);
testBufs('\u0222aa', 8);
testBufs('a\u0234b\u0235c\u0236', 4);
testBufs('a\u0234b\u0235c\u0236', 12);
testBufs('abc', 4, 1);
testBufs('abc', 5, 1);
testBufs('\u0222aa', 8, 1);
testBufs('a\u0234b\u0235c\u0236', 4, 1);
testBufs('a\u0234b\u0235c\u0236', 12, 1);

// UTF8
testBufs('abc', 'utf8');
testBufs('\u0222aa', 'utf8');
testBufs('a\u0234b\u0235c\u0236', 'utf8');
testBufs('abc', 4, 'utf8');
testBufs('abc', 5, 'utf8');
testBufs('abc', SIZE, 'utf8');
testBufs('\u0222aa', 2, 'utf8');
testBufs('\u0222aa', 8, 'utf8');
testBufs('a\u0234b\u0235c\u0236', 4, 'utf8');
testBufs('a\u0234b\u0235c\u0236', 12, 'utf8');
testBufs('abc', 4, 1, 'utf8');
testBufs('abc', 5, 1, 'utf8');
testBufs('\u0222aa', 8, 1, 'utf8');
testBufs('a\u0234b\u0235c\u0236', 4, 1, 'utf8');
testBufs('a\u0234b\u0235c\u0236', 12, 1, 'utf8');
assert.strictEqual(Buffer.allocUnsafe(1).fill(0).fill('\u0222')[0], 0xc8);

// BINARY
testBufs('abc', 'binary');
testBufs('\u0222aa', 'binary');
testBufs('a\u0234b\u0235c\u0236', 'binary');
testBufs('abc', 4, 'binary');
testBufs('abc', 5, 'binary');
testBufs('abc', SIZE, 'binary');
testBufs('\u0222aa', 2, 'binary');
testBufs('\u0222aa', 8, 'binary');
testBufs('a\u0234b\u0235c\u0236', 4, 'binary');
testBufs('a\u0234b\u0235c\u0236', 12, 'binary');
testBufs('abc', 4, 1, 'binary');
testBufs('abc', 5, 1, 'binary');
testBufs('\u0222aa', 8, 1, 'binary');
testBufs('a\u0234b\u0235c\u0236', 4, 1, 'binary');
testBufs('a\u0234b\u0235c\u0236', 12, 1, 'binary');

// LATIN1
testBufs('abc', 'latin1');
testBufs('\u0222aa', 'latin1');
testBufs('a\u0234b\u0235c\u0236', 'latin1');
testBufs('abc', 4, 'latin1');
testBufs('abc', 5, 'latin1');
testBufs('abc', SIZE, 'latin1');
testBufs('\u0222aa', 2, 'latin1');
testBufs('\u0222aa', 8, 'latin1');
testBufs('a\u0234b\u0235c\u0236', 4, 'latin1');
testBufs('a\u0234b\u0235c\u0236', 12, 'latin1');
testBufs('abc', 4, 1, 'latin1');
testBufs('abc', 5, 1, 'latin1');
testBufs('\u0222aa', 8, 1, 'latin1');
testBufs('a\u0234b\u0235c\u0236', 4, 1, 'latin1');
testBufs('a\u0234b\u0235c\u0236', 12, 1, 'latin1');

// UCS2
testBufs('abc', 'ucs2');
testBufs('\u0222aa', 'ucs2');
testBufs('a\u0234b\u0235c\u0236', 'ucs2');
testBufs('abc', 4, 'ucs2');
testBufs('abc', SIZE, 'ucs2');
testBufs('\u0222aa', 2, 'ucs2');
testBufs('\u0222aa', 8, 'ucs2');
testBufs('a\u0234b\u0235c\u0236', 4, 'ucs2');
testBufs('a\u0234b\u0235c\u0236', 12, 'ucs2');
testBufs('abc', 4, 1, 'ucs2');
testBufs('abc', 5, 1, 'ucs2');
testBufs('\u0222aa', 8, 1, 'ucs2');
testBufs('a\u0234b\u0235c\u0236', 4, 1, 'ucs2');
testBufs('a\u0234b\u0235c\u0236', 12, 1, 'ucs2');
assert.strictEqual(Buffer.allocUnsafe(1).fill('\u0222', 'ucs2')[0], 0x22);

// HEX
testBufs('616263', 'hex');
testBufs('c8a26161', 'hex');
testBufs('61c8b462c8b563c8b6', 'hex');
testBufs('616263', 4, 'hex');
testBufs('616263', 5, 'hex');
testBufs('616263', SIZE, 'hex');
testBufs('c8a26161', 2, 'hex');
testBufs('c8a26161', 8, 'hex');
testBufs('61c8b462c8b563c8b6', 4, 'hex');
testBufs('61c8b462c8b563c8b6', 12, 'hex');
testBufs('616263', 4, 1, 'hex');
testBufs('616263', 5, 1, 'hex');
testBufs('c8a26161', 8, 1, 'hex');
testBufs('61c8b462c8b563c8b6', 4, 1, 'hex');
testBufs('61c8b462c8b563c8b6', 12, 1, 'hex');

common.expectsError(() => {
  const buf = Buffer.allocUnsafe(SIZE);

  buf.fill('yKJh', 'hex');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  type: TypeError
});

common.expectsError(() => {
  const buf = Buffer.allocUnsafe(SIZE);

  buf.fill('\u0222', 'hex');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  type: TypeError
});

// BASE64
testBufs('YWJj', 'ucs2');
testBufs('yKJhYQ==', 'ucs2');
testBufs('Yci0Ysi1Y8i2', 'ucs2');
testBufs('YWJj', 4, 'ucs2');
testBufs('YWJj', SIZE, 'ucs2');
testBufs('yKJhYQ==', 2, 'ucs2');
testBufs('yKJhYQ==', 8, 'ucs2');
testBufs('Yci0Ysi1Y8i2', 4, 'ucs2');
testBufs('Yci0Ysi1Y8i2', 12, 'ucs2');
testBufs('YWJj', 4, 1, 'ucs2');
testBufs('YWJj', 5, 1, 'ucs2');
testBufs('yKJhYQ==', 8, 1, 'ucs2');
testBufs('Yci0Ysi1Y8i2', 4, 1, 'ucs2');
testBufs('Yci0Ysi1Y8i2', 12, 1, 'ucs2');

// Buffer
function deepStrictEqualValues(buf, arr) {
  for (const [index, value] of buf.entries()) {
    assert.deepStrictEqual(value, arr[index]);
  }
}

const buf2Fill = Buffer.allocUnsafe(1).fill(2);
deepStrictEqualValues(genBuffer(4, [buf2Fill]), [2, 2, 2, 2]);
deepStrictEqualValues(genBuffer(4, [buf2Fill, 1]), [0, 2, 2, 2]);
deepStrictEqualValues(genBuffer(4, [buf2Fill, 1, 3]), [0, 2, 2, 0]);
deepStrictEqualValues(genBuffer(4, [buf2Fill, 1, 1]), [0, 0, 0, 0]);
const hexBufFill = Buffer.allocUnsafe(2).fill(0).fill('0102', 'hex');
deepStrictEqualValues(genBuffer(4, [hexBufFill]), [1, 2, 1, 2]);
deepStrictEqualValues(genBuffer(4, [hexBufFill, 1]), [0, 1, 2, 1]);
deepStrictEqualValues(genBuffer(4, [hexBufFill, 1, 3]), [0, 1, 2, 0]);
deepStrictEqualValues(genBuffer(4, [hexBufFill, 1, 1]), [0, 0, 0, 0]);

// Check exceptions
[
  [0, -1],
  [0, 0, buf1.length + 1],
  ['', -1],
  ['', 0, buf1.length + 1],
  ['', 1, -1],
].forEach((args) => {
  common.expectsError(
    () => buf1.fill(...args),
    { code: 'ERR_OUT_OF_RANGE' }
  );
});

common.expectsError(
  () => buf1.fill('a', 0, buf1.length, 'node rocks!'),
  {
    code: 'ERR_UNKNOWN_ENCODING',
    type: TypeError,
    message: 'Unknown encoding: node rocks!'
  }
);

[
  ['a', 0, 0, NaN],
  ['a', 0, 0, false]
].forEach((args) => {
  common.expectsError(
    () => buf1.fill(...args),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "encoding" argument must be of type ' +
      `string. Received type ${args[3] === null ? 'null' : typeof args[3]}`
    }
  );
});

common.expectsError(
  () => buf1.fill('a', 0, 0, 'foo'),
  {
    code: 'ERR_UNKNOWN_ENCODING',
    type: TypeError,
    message: 'Unknown encoding: foo'
  }
);

function genBuffer(size, args) {
  const b = Buffer.allocUnsafe(size);
  return b.fill(0).fill.apply(b, args);
}

function bufReset() {
  buf1.fill(0);
  buf2.fill(0);
}

// This is mostly accurate. Except write() won't write partial bytes to the
// string while fill() blindly copies bytes into memory. To account for that an
// error will be thrown if not all the data can be written, and the SIZE has
// been massaged to work with the input characters.
function writeToFill(string, offset, end, encoding) {
  if (typeof offset === 'string') {
    encoding = offset;
    offset = 0;
    end = buf2.length;
  } else if (typeof end === 'string') {
    encoding = end;
    end = buf2.length;
  } else if (end === undefined) {
    end = buf2.length;
  }

  // Should never be reached.
  if (offset < 0 || end > buf2.length)
    throw new ERR_OUT_OF_RANGE();

  if (end <= offset)
    return buf2;

  offset >>>= 0;
  end >>>= 0;
  assert(offset <= buf2.length);

  // Convert "end" to "length" (which write understands).
  const length = end - offset < 0 ? 0 : end - offset;

  let wasZero = false;
  do {
    const written = buf2.write(string, offset, length, encoding);
    offset += written;
    // Safety check in case write falls into infinite loop.
    if (written === 0) {
      if (wasZero)
        throw new Error('Could not write all data to Buffer');
      else
        wasZero = true;
    }
  } while (offset < buf2.length);

  return buf2;
}

function testBufs(string, offset, length, encoding) {
  bufReset();
  buf1.fill.apply(buf1, arguments);
  // Swap bytes on BE archs for ucs2 encoding.
  assert.deepStrictEqual(buf1.fill.apply(buf1, arguments),
                         writeToFill.apply(null, arguments));
}

// Make sure these throw.
common.expectsError(
  () => Buffer.allocUnsafe(8).fill('a', -1),
  { code: 'ERR_OUT_OF_RANGE' });
common.expectsError(
  () => Buffer.allocUnsafe(8).fill('a', 0, 9),
  { code: 'ERR_OUT_OF_RANGE' });

// Make sure this doesn't hang indefinitely.
Buffer.allocUnsafe(8).fill('');
Buffer.alloc(8, '');

{
  const buf = Buffer.alloc(64, 10);
  for (let i = 0; i < buf.length; i++)
    assert.strictEqual(buf[i], 10);

  buf.fill(11, 0, buf.length >> 1);
  for (let i = 0; i < buf.length >> 1; i++)
    assert.strictEqual(buf[i], 11);
  for (let i = (buf.length >> 1) + 1; i < buf.length; i++)
    assert.strictEqual(buf[i], 10);

  buf.fill('h');
  for (let i = 0; i < buf.length; i++)
    assert.strictEqual('h'.charCodeAt(0), buf[i]);

  buf.fill(0);
  for (let i = 0; i < buf.length; i++)
    assert.strictEqual(buf[i], 0);

  buf.fill(null);
  for (let i = 0; i < buf.length; i++)
    assert.strictEqual(buf[i], 0);

  buf.fill(1, 16, 32);
  for (let i = 0; i < 16; i++)
    assert.strictEqual(buf[i], 0);
  for (let i = 16; i < 32; i++)
    assert.strictEqual(buf[i], 1);
  for (let i = 32; i < buf.length; i++)
    assert.strictEqual(buf[i], 0);
}

{
  const buf = Buffer.alloc(10, 'abc');
  assert.strictEqual(buf.toString(), 'abcabcabca');
  buf.fill('է');
  assert.strictEqual(buf.toString(), 'էէէէէ');
}

// Testing process.binding. Make sure "start" is properly checked for -1 wrap
// around.
assert.strictEqual(
  internalBinding('buffer').fill(Buffer.alloc(1), 1, -1, 0, 1), -2);

// Make sure "end" is properly checked, even if it's magically mangled using
// Symbol.toPrimitive.
{
  let elseWasLast = false;
  const expectedErrorMessage =
    'The value of "end" is out of range. It must be >= 0 and <= 1. Received -1';

  common.expectsError(() => {
    let ctr = 0;
    const end = {
      [Symbol.toPrimitive]() {
        // We use this condition to get around the check in lib/buffer.js
        if (ctr === 0) {
          elseWasLast = false;
          ctr++;
          return 1;
        }
        elseWasLast = true;
        // Once buffer.js calls the C++ implementation of fill, return -1
        return -1;
      }
    };
    Buffer.alloc(1).fill(Buffer.alloc(1), 0, end);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    type: RangeError,
    message: expectedErrorMessage
  });
  // Make sure -1 is making it to Buffer::Fill().
  assert.ok(elseWasLast,
            'internal API changed, -1 no longer in correct location');
}

// Testing process.binding. Make sure "end" is properly checked for -1 wrap
// around.
assert.strictEqual(
  internalBinding('buffer').fill(Buffer.alloc(1), 1, 1, -2, 1), -2);

// Test that bypassing 'length' won't cause an abort.
common.expectsError(() => {
  const buf = Buffer.from('w00t');
  Object.defineProperty(buf, 'length', {
    value: 1337,
    enumerable: true
  });
  buf.fill('');
}, {
  code: 'ERR_BUFFER_OUT_OF_BOUNDS',
  type: RangeError,
  message: 'Attempt to write outside buffer bounds'
});

assert.deepStrictEqual(
  Buffer.allocUnsafeSlow(16).fill('ab', 'utf16le'),
  Buffer.from('61006200610062006100620061006200', 'hex'));

assert.deepStrictEqual(
  Buffer.allocUnsafeSlow(15).fill('ab', 'utf16le'),
  Buffer.from('610062006100620061006200610062', 'hex'));

assert.deepStrictEqual(
  Buffer.allocUnsafeSlow(16).fill('ab', 'utf16le'),
  Buffer.from('61006200610062006100620061006200', 'hex'));
assert.deepStrictEqual(
  Buffer.allocUnsafeSlow(16).fill('a', 'utf16le'),
  Buffer.from('61006100610061006100610061006100', 'hex'));

assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('a', 'utf16le').toString('utf16le'),
  'a'.repeat(8));
assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('a', 'latin1').toString('latin1'),
  'a'.repeat(16));
assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('a', 'utf8').toString('utf8'),
  'a'.repeat(16));

assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('Љ', 'utf16le').toString('utf16le'),
  'Љ'.repeat(8));
assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('Љ', 'latin1').toString('latin1'),
  '\t'.repeat(16));
assert.strictEqual(
  Buffer.allocUnsafeSlow(16).fill('Љ', 'utf8').toString('utf8'),
  'Љ'.repeat(8));

common.expectsError(() => {
  const buf = Buffer.from('a'.repeat(1000));

  buf.fill('This is not correctly encoded', 'hex');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
  type: TypeError
});
