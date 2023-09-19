// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { Writable } = require('stream');
const { CSI } = require('internal/readline/utils');

{
  assert(CSI);
  assert.strictEqual(CSI.kClearToLineBeginning, '\x1b[1K');
  assert.strictEqual(CSI.kClearToLineEnd, '\x1b[0K');
  assert.strictEqual(CSI.kClearLine, '\x1b[2K');
  assert.strictEqual(CSI.kClearScreenDown, '\x1b[0J');
  assert.strictEqual(CSI`1${2}3`, '\x1b[123');
}

class TestWritable extends Writable {
  constructor() {
    super();
    this.data = '';
  }
  _write(chunk, encoding, callback) {
    this.data += chunk.toString();
    callback();
  }
}

const writable = new TestWritable();

assert.strictEqual(readline.clearScreenDown(writable), true);
assert.deepStrictEqual(writable.data, CSI.kClearScreenDown);
assert.strictEqual(readline.clearScreenDown(writable, common.mustCall()), true);

// Verify that clearScreenDown() throws on invalid callback.
assert.throws(() => {
  readline.clearScreenDown(writable, null);
}, /ERR_INVALID_ARG_TYPE/);

// Verify that clearScreenDown() does not throw on null or undefined stream.
assert.strictEqual(readline.clearScreenDown(null, common.mustCall((err) => {
  assert.strictEqual(err, null);
})), true);
assert.strictEqual(readline.clearScreenDown(undefined, common.mustCall()),
                   true);

writable.data = '';
assert.strictEqual(readline.clearLine(writable, -1), true);
assert.deepStrictEqual(writable.data, CSI.kClearToLineBeginning);

writable.data = '';
assert.strictEqual(readline.clearLine(writable, 1), true);
assert.deepStrictEqual(writable.data, CSI.kClearToLineEnd);

writable.data = '';
assert.strictEqual(readline.clearLine(writable, 0), true);
assert.deepStrictEqual(writable.data, CSI.kClearLine);

writable.data = '';
assert.strictEqual(readline.clearLine(writable, -1, common.mustCall()), true);
assert.deepStrictEqual(writable.data, CSI.kClearToLineBeginning);

// Verify that clearLine() throws on invalid callback.
assert.throws(() => {
  readline.clearLine(writable, 0, null);
}, /ERR_INVALID_ARG_TYPE/);

// Verify that clearLine() does not throw on null or undefined stream.
assert.strictEqual(readline.clearLine(null, 0), true);
assert.strictEqual(readline.clearLine(undefined, 0), true);
assert.strictEqual(readline.clearLine(null, 0, common.mustCall((err) => {
  assert.strictEqual(err, null);
})), true);
assert.strictEqual(readline.clearLine(undefined, 0, common.mustCall()), true);

// Nothing is written when moveCursor 0, 0
[
  [0, 0, ''],
  [1, 0, '\x1b[1C'],
  [-1, 0, '\x1b[1D'],
  [0, 1, '\x1b[1B'],
  [0, -1, '\x1b[1A'],
  [1, 1, '\x1b[1C\x1b[1B'],
  [-1, 1, '\x1b[1D\x1b[1B'],
  [-1, -1, '\x1b[1D\x1b[1A'],
  [1, -1, '\x1b[1C\x1b[1A'],
].forEach((set) => {
  writable.data = '';
  assert.strictEqual(readline.moveCursor(writable, set[0], set[1]), true);
  assert.deepStrictEqual(writable.data, set[2]);
  writable.data = '';
  assert.strictEqual(
    readline.moveCursor(writable, set[0], set[1], common.mustCall()),
    true
  );
  assert.deepStrictEqual(writable.data, set[2]);
});

// Verify that moveCursor() throws on invalid callback.
assert.throws(() => {
  readline.moveCursor(writable, 1, 1, null);
}, /ERR_INVALID_ARG_TYPE/);

// Verify that moveCursor() does not throw on null or undefined stream.
assert.strictEqual(readline.moveCursor(null, 1, 1), true);
assert.strictEqual(readline.moveCursor(undefined, 1, 1), true);
assert.strictEqual(readline.moveCursor(null, 1, 1, common.mustCall((err) => {
  assert.strictEqual(err, null);
})), true);
assert.strictEqual(readline.moveCursor(undefined, 1, 1, common.mustCall()),
                   true);

// Undefined or null as stream should not throw.
assert.strictEqual(readline.cursorTo(null), true);
assert.strictEqual(readline.cursorTo(), true);
assert.strictEqual(readline.cursorTo(null, 1, 1, common.mustCall()), true);
assert.strictEqual(readline.cursorTo(undefined, 1, 1, common.mustCall((err) => {
  assert.strictEqual(err, null);
})), true);

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 'a'), true);
assert.strictEqual(writable.data, '');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 'a', 'b'), true);
assert.strictEqual(writable.data, '');

writable.data = '';
assert.throws(
  () => readline.cursorTo(writable, 'a', 1),
  {
    name: 'TypeError',
    code: 'ERR_INVALID_CURSOR_POS',
    message: 'Cannot set cursor row without setting its column'
  });
assert.strictEqual(writable.data, '');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 1, 'a'), true);
assert.strictEqual(writable.data, '\x1b[2G');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 1), true);
assert.strictEqual(writable.data, '\x1b[2G');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 1, 2), true);
assert.strictEqual(writable.data, '\x1b[3;2H');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 1, 2, common.mustCall()), true);
assert.strictEqual(writable.data, '\x1b[3;2H');

writable.data = '';
assert.strictEqual(readline.cursorTo(writable, 1, common.mustCall()), true);
assert.strictEqual(writable.data, '\x1b[2G');

// Verify that cursorTo() throws on invalid callback.
assert.throws(() => {
  readline.cursorTo(writable, 1, 1, null);
}, /ERR_INVALID_ARG_TYPE/);

// Verify that cursorTo() throws if x or y is NaN.
assert.throws(() => {
  readline.cursorTo(writable, NaN);
}, /ERR_INVALID_ARG_VALUE/);

assert.throws(() => {
  readline.cursorTo(writable, 1, NaN);
}, /ERR_INVALID_ARG_VALUE/);

assert.throws(() => {
  readline.cursorTo(writable, NaN, NaN);
}, /ERR_INVALID_ARG_VALUE/);
