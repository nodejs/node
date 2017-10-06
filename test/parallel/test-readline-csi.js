// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { Writable } = require('stream');
const { CSI } = require('internal/readline');

{
  assert(CSI);
  assert.strictEqual(CSI.kClearToBeginning, '\x1b[1K');
  assert.strictEqual(CSI.kClearToEnd, '\x1b[0K');
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

readline.clearScreenDown(writable);
assert.deepStrictEqual(writable.data, CSI.kClearScreenDown);

writable.data = '';
readline.clearLine(writable, -1);
assert.deepStrictEqual(writable.data, CSI.kClearToBeginning);

writable.data = '';
readline.clearLine(writable, 1);
assert.deepStrictEqual(writable.data, CSI.kClearToEnd);

writable.data = '';
readline.clearLine(writable, 0);
assert.deepStrictEqual(writable.data, CSI.kClearLine);

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
  readline.moveCursor(writable, set[0], set[1]);
  assert.deepStrictEqual(writable.data, set[2]);
});

assert.doesNotThrow(() => readline.cursorTo(null));
assert.doesNotThrow(() => readline.cursorTo());

writable.data = '';
assert.doesNotThrow(() => readline.cursorTo(writable, 'a'));
assert.strictEqual(writable.data, '');

writable.data = '';
assert.doesNotThrow(() => readline.cursorTo(writable, 'a', 'b'));
assert.strictEqual(writable.data, '');

writable.data = '';
assert.throws(
  () => readline.cursorTo(writable, 'a', 1),
  common.expectsError({
    type: Error,
    code: 'ERR_INVALID_CURSOR_POS',
    message: 'Cannot set cursor row without setting its column'
  }));
assert.strictEqual(writable.data, '');

writable.data = '';
assert.doesNotThrow(() => readline.cursorTo(writable, 1, 'a'));
assert.strictEqual(writable.data, '\x1b[2G');

writable.data = '';
assert.doesNotThrow(() => readline.cursorTo(writable, 1, 2));
assert.strictEqual(writable.data, '\x1b[3;2H');
