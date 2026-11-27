'use strict';
require('../common');
const assert = require('node:assert');
const readline = require('node:readline');
const { PassThrough } = require('node:stream');

// Test terminal write with unicodeLineSeparators: false
{
  const input = new PassThrough();
  const output = new PassThrough();

  const rli = readline.createInterface({
    input,
    output,
    terminal: true,
    unicodeLineSeparators: false,
  });

  const lines = [];
  rli.on('line', (line) => lines.push(line));

  input.write('foo\u2028bar\n');
  rli.close();

  assert.deepStrictEqual(lines, ['foo\u2028bar']);
}

// Test terminal write with default unicodeLineSeparators: true
{
  const input = new PassThrough();
  const output = new PassThrough();

  const rli = readline.createInterface({
    input,
    output,
    terminal: true,
  });

  const lines = [];
  rli.on('line', (line) => lines.push(line));

  input.write('foo\u2028bar\n');
  rli.close();

  assert.deepStrictEqual(lines, ['foo', 'bar']);
}
