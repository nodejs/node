'use strict';
const common = require('../common');
const assert = require('node:assert');
const readline = require('node:readline');
const { Readable } = require('node:stream');

// Test 1: JSONL with Unicode line/paragraph separators inside JSON strings
{
  const jsonlData = '{"text":"Hello\\u2028World"}\n{"text":"Foo\\u2029Bar"}\n';

  const rli = readline.createInterface({
    input: Readable.from(jsonlData),
    unicodeLineSeparators: false,
  });

  const parsed = [];
  rli.on('line', (line) => {
    parsed.push(JSON.parse(line));
  });

  rli.on('close', common.mustCall(() => {
    assert.strictEqual(parsed.length, 2);
    assert.deepStrictEqual(parsed[0], { text: 'Hello\u2028World' });
    assert.deepStrictEqual(parsed[1], { text: 'Foo\u2029Bar' });
  }));
}
