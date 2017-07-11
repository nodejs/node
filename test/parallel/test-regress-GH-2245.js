/* eslint-disable strict */
require('../common');
const assert = require('assert');

/*
In Node.js 0.10, a bug existed that caused strict functions to not capture
their environment when evaluated. When run in 0.10 `test()` fails with a
`ReferenceError`. See https://github.com/nodejs/node/issues/2245 for details.
*/

function test() {

  const code = [
    'var foo = {m: 1};',
    '',
    'function bar() {',
    '\'use strict\';',
    'return foo; // foo isn\'t captured in 0.10',
    '};'
  ].join('\n');

  eval(code);

  return bar(); // eslint-disable-line no-undef

}

assert.deepStrictEqual(test(), { m: 1 });
