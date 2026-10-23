'use strict';
const common = require('../common');
const assert = require('node:assert');
const readline = require('node:readline');
const { Readable } = require('node:stream');

// Test async iterator on readline interface with unicodeLineSeparators: false
(async () => {
  const input = 'line 1\u2028continued\nline 2\u2029continued\nline 3';
  const rli = readline.createInterface({
    input: Readable.from(input),
    unicodeLineSeparators: false,
  });

  const lines = [];
  for await (const line of rli) {
    lines.push(line);
  }

  assert.deepStrictEqual(lines, [
    'line 1\u2028continued',
    'line 2\u2029continued',
    'line 3',
  ]);
})().then(common.mustCall());
