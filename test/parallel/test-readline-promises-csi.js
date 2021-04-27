// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { Readline } = require('readline/promises');
const { Writable } = require('stream');
const { CSI } = require('internal/readline/utils');

const INVALID_ARG = {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
};
class TestWritable extends Writable {
  data = '';
  _write(chunk, encoding, callback) {
    this.data += chunk.toString();
    callback();
  }
}

[
  undefined, null,
  0, 1, 1n, 1.1, NaN, Infinity,
  true, false,
  Symbol(),
  '', '1',
  [], {}, () => {},
].forEach((arg) =>
  assert.throws(() => new Readline(arg), INVALID_ARG)
);

(async function test() {
  const writable = new TestWritable();
  const readline = new Readline(writable);

  await readline.clearScreenDown().commit();
  assert.deepStrictEqual(writable.data, CSI.kClearScreenDown);
  await readline.clearScreenDown().commit();

  writable.data = '';
  await readline.clearScreenDown().rollback();
  assert.deepStrictEqual(writable.data, '');

  writable.data = '';
  await readline.clearLine(-1).commit();
  assert.deepStrictEqual(writable.data, CSI.kClearToLineBeginning);

  writable.data = '';
  await readline.clearLine(1).commit();
  assert.deepStrictEqual(writable.data, CSI.kClearToLineEnd);

  writable.data = '';
  await readline.clearLine(0).commit();
  assert.deepStrictEqual(writable.data, CSI.kClearLine);

  writable.data = '';
  await readline.clearLine(-1).commit();
  assert.deepStrictEqual(writable.data, CSI.kClearToLineBeginning);

  await readline.clearLine(0, null).commit();

  // Nothing is written when moveCursor 0, 0
  for (const set of
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
    ]) {
    writable.data = '';
    await readline.moveCursor(set[0], set[1]).commit();
    assert.deepStrictEqual(writable.data, set[2]);
    writable.data = '';
    await readline.moveCursor(set[0], set[1]).commit();
    assert.deepStrictEqual(writable.data, set[2]);
  }


  await readline.moveCursor(1, 1, null).commit();

  writable.data = '';
  [
    undefined, null,
    true, false,
    Symbol(),
    '', '1',
    [], {}, () => {},
  ].forEach((arg) =>
    assert.throws(() => readline.cursorTo(arg), INVALID_ARG)
  );
  assert.strictEqual(writable.data, '');

  writable.data = '';
  assert.throws(() => readline.cursorTo('a', 'b'), INVALID_ARG);
  assert.strictEqual(writable.data, '');

  writable.data = '';
  assert.throws(() => readline.cursorTo('a', 1), INVALID_ARG);
  assert.strictEqual(writable.data, '');

  writable.data = '';
  assert.throws(() => readline.cursorTo(1, 'a'), INVALID_ARG);
  assert.strictEqual(writable.data, '');

  writable.data = '';
  await readline.cursorTo(1).commit();
  assert.strictEqual(writable.data, '\x1b[2G');

  writable.data = '';
  await readline.cursorTo(1, 2).commit();
  assert.strictEqual(writable.data, '\x1b[3;2H');

  writable.data = '';
  await readline.cursorTo(1, 2).commit();
  assert.strictEqual(writable.data, '\x1b[3;2H');

  writable.data = '';
  await readline.cursorTo(1).cursorTo(1, 2).commit();
  assert.strictEqual(writable.data, '\x1b[2G\x1b[3;2H');

  writable.data = '';
  await readline.cursorTo(1).commit();
  assert.strictEqual(writable.data, '\x1b[2G');

  // Verify that cursorTo() rejects if x or y is NaN.
  [1.1, NaN, Infinity].forEach((arg) => {
    assert.throws(() => readline.cursorTo(arg), {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    });
  });

  [1.1, NaN, Infinity].forEach((arg) => {
    assert.throws(() => readline.cursorTo(1, arg), {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    });
  });

  assert.throws(() => readline.cursorTo(NaN, NaN), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  });
})().then(common.mustCall());
