'use strict';
const common = require('../common');
const assert = require('node:assert');
const readlinePromises = require('node:readline/promises');
const { Readable } = require('node:stream');

// Test readlinePromises.createInterface with unicodeLineSeparators: false
{
  const input = '{"a":1}\u2028{"b":2}\n{"c":3}';
  const rli = readlinePromises.createInterface({
    input: Readable.from(input),
    unicodeLineSeparators: false,
  });

  const lines = [];
  rli.on('line', (line) => lines.push(line));

  rli.on('close', common.mustCall(() => {
    assert.deepStrictEqual(lines, ['{"a":1}\u2028{"b":2}', '{"c":3}']);
  }));
}
