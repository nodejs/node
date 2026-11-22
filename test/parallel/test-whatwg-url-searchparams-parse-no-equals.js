'use strict';

// Pairs with no '=' mixed with pairs that have one.

require('../common');
const assert = require('node:assert');

const cases = [
  ['a&b&c=d', [['a', ''], ['b', ''], ['c', 'd']]],
  ['a=b&c&d&e=f&g', [['a', 'b'], ['c', ''], ['d', ''], ['e', 'f'], ['g', '']]],
  ['a&&b=c=d&&e', [['a', ''], ['b', 'c=d'], ['e', '']]],
  ['=&a&=b', [['', ''], ['a', ''], ['', 'b']]],
  ['?a&b=', [['a', ''], ['b', '']]],
  ['a+b&c%20d=e+f&g', [['a b', ''], ['c d', 'e f'], ['g', '']]],
  [`${'a&'.repeat(50)}x=y`, [...Array(50).fill(['a', '']), ['x', 'y']]],
];

for (const [input, expected] of cases) {
  assert.deepStrictEqual([...new URLSearchParams(input)], expected);
}

{
  const pairs = 1 << 18;
  const params = new URLSearchParams(`${'a&'.repeat(pairs)}x=y`);
  assert.strictEqual(params.size, pairs + 1);
  assert.strictEqual(params.get('x'), 'y');
}
