'use strict';
const common = require('../common');
const assert = require('node:assert');
const readline = require('node:readline');
const { Readable } = require('node:stream');

const str = '012\n345\r67\r\n89\u{2028}ABC\u{2029}DEF';

const rli = new readline.Interface({
  input: Readable.from(str),
});

const linesRead = [];
rli.on('line', (line) => linesRead.push(line));

rli.on('close', common.mustCall(() => {
  assert.deepStrictEqual(linesRead, ['012', '345', '67', '89', 'ABC', 'DEF']);
}));
