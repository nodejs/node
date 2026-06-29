'use strict';
const common = require('../common');
const assert = require('node:assert');
const readline = require('node:readline');
const { Readable } = require('node:stream');

const str = '012\n345\r67\r\n89\u{2028}ABC\u{2029}DEF';

// Unicode line and paragraph separators are line endings by default.
{
  const rli = new readline.Interface({
    input: Readable.from(str),
  });

  const linesRead = [];
  rli.on('line', (line) => linesRead.push(line));

  rli.on('close', common.mustCall(() => {
    assert.deepStrictEqual(linesRead, ['012', '345', '67', '89', 'ABC', 'DEF']);
  }));
}

// The option allows file formats such as JSONL to keep Unicode separators
// inside record contents while still splitting on CR, LF, and CRLF.
{
  const rli = new readline.Interface({
    input: Readable.from(str),
    unicodeLineSeparators: false,
  });

  const linesRead = [];
  rli.on('line', (line) => linesRead.push(line));

  rli.on('close', common.mustCall(() => {
    assert.deepStrictEqual(
      linesRead,
      ['012', '345', '67', '89\u2028ABC\u2029DEF'],
    );
  }));
}

assert.throws(
  () => new readline.Interface({
    input: Readable.from(str),
    unicodeLineSeparators: 'false',
  }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
  },
);
