'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const repl = require('repl');


function run({ command, expected }) {
  let accum = '';

  const inputStream = new common.ArrayStream();
  const outputStream = new common.ArrayStream();

  outputStream.write = (data) => accum += data.replace('\r', '');

  const r = repl.start({
    prompt: '',
    input: inputStream,
    output: outputStream,
    terminal: false,
    useColors: false
  });

  r.write(`${command}\n`);
  assert.strictEqual(accum, expected);
  r.close();
}

const tests = [
  {
    // test .load for a file that throws
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: `Error: Whoops!
    at c (repl:9:9)
    at b (repl:6:3)
    at a (repl:3:3)
`
  },
  {
    command: 'let x y;',
    expected: `let x y;
      ^

SyntaxError: Unexpected identifier

`
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Error: Whoops!\n'
  },
  {
    command: 'foo = bar;',
    expected: 'ReferenceError: bar is not defined\n'
  }
];

tests.forEach(run);
