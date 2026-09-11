'use strict';
const common = require('../common');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

// Regression test: when moving the cursor vertically through a line that is too
// short to hold the current column, readline "remembers" the column and should
// restore it once a subsequent line is long enough. The remembered column is a
// visual column that includes the continuation-prompt width, so it must be
// compared against the target line length plus that prompt width. Previously
// the comparison omitted the prompt width, so the cursor incorrectly clamped to
// the end of the line for columns in the (line length, line length + prompt]
// range.

const { PassThrough } = require('stream');
const readline = require('readline');
const assert = require('assert');

const input = new PassThrough();
const output = new PassThrough();
output.columns = 80;
output.isTTY = true;

// The history entry uses '\r' as the line separator; it is displayed as three
// lines: "aaaaa" / "bb" / "cccccc".
const rl = readline.createInterface({
  input,
  output,
  terminal: true,
  prompt: '> ',
  history: ['cccccc\rbb\raaaaa'],
});

// Load the multiline history entry.
rl.write(null, { name: 'up' });
assert.strictEqual(rl.line, 'aaaaa\nbb\ncccccc');

// Place the cursor at visual column 6 on the bottom line ("cccccc"), which is
// 4 characters into that line.
rl.cursor = 13;
assert.deepStrictEqual(rl.getCursorPos(), { cols: 6, rows: 2 });

// Move up onto "bb". It is too short for column 6, so the cursor clamps to the
// end of "bb" and column 6 is remembered.
rl.write(null, { name: 'up' });
assert.deepStrictEqual(rl.getCursorPos(), { cols: 4, rows: 1 });

// Move up onto "aaaaa". Column 6 is reachable here (its columns span 2..7), so
// the remembered column must be restored instead of clamping to the end.
rl.write(null, { name: 'up' });
assert.strictEqual(rl.cursor, 4);
assert.deepStrictEqual(rl.getCursorPos(), { cols: 6, rows: 0 });

rl.close();
