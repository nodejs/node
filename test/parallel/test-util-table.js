'use strict';

require('../common');

const assert = require('assert');
const util = require('util');

// `util.table` returns the rendered table as a string instead of writing it.
assert.strictEqual(typeof util.table([{ a: 1 }]), 'string');

// Array of objects.
assert.strictEqual(
  util.table([{ a: 1, b: 'Y' }, { a: 'Z', b: 2 }]),
  '┌─────────┬─────┬─────┐\n' +
  '│ (index) │ a   │ b   │\n' +
  '├─────────┼─────┼─────┤\n' +
  "│ 0       │ 1   │ 'Y' │\n" +
  "│ 1       │ 'Z' │ 2   │\n" +
  '└─────────┴─────┴─────┘',
);

// Properties subset selects the columns to render.
assert.strictEqual(
  util.table([{ a: 1, b: 'Y' }, { a: 'Z', b: 2 }], ['a']),
  '┌─────────┬─────┐\n' +
  '│ (index) │ a   │\n' +
  '├─────────┼─────┤\n' +
  '│ 0       │ 1   │\n' +
  "│ 1       │ 'Z' │\n" +
  '└─────────┴─────┘',
);

// Map.
assert.strictEqual(
  util.table(new Map([['a', 1], ['b', 2]])),
  '┌───────────────────┬─────┬────────┐\n' +
  '│ (iteration index) │ Key │ Values │\n' +
  '├───────────────────┼─────┼────────┤\n' +
  "│ 0                 │ 'a' │ 1      │\n" +
  "│ 1                 │ 'b' │ 2      │\n" +
  '└───────────────────┴─────┴────────┘',
);

// Set.
assert.strictEqual(
  util.table(new Set([1, 2])),
  '┌───────────────────┬────────┐\n' +
  '│ (iteration index) │ Values │\n' +
  '├───────────────────┼────────┤\n' +
  '│ 0                 │ 1      │\n' +
  '│ 1                 │ 2      │\n' +
  '└───────────────────┴────────┘',
);

// Non-tabular input falls back to `util.inspect`.
assert.strictEqual(util.table(42), '42');
assert.strictEqual(util.table('hi'), "'hi'");
assert.strictEqual(util.table(null), 'null');

// `options` is forwarded to inspect for each cell.
assert.strictEqual(
  util.table([{ a: 'abcdef' }], undefined, { maxStringLength: 3 }),
  '┌─────────┬────────────────────────────┐\n' +
  '│ (index) │ a                          │\n' +
  '├─────────┼────────────────────────────┤\n' +
  "│ 0       │ 'abc'... 3 more characters │\n" +
  '└─────────┴────────────────────────────┘',
);

// Argument validation.
assert.throws(() => util.table([], 'nope'), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});
assert.throws(() => util.table([], undefined, 'nope'), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});
