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
    expected: 'Error: Whoops!\n    at repl:9:24\n    at d (repl:12:3)\n    ' +
              'at c (repl:9:3)\n    at b (repl:6:3)\n    at a (repl:3:3)\n'
  },
  {
    command: 'let x y;',
    expected: 'let x y;\n      ^\n\nSyntaxError: Unexpected identifier\n\n'
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Error: Whoops!\n'
  },
  {
    command: 'foo = bar;',
    expected: 'ReferenceError: bar is not defined\n'
  },
  // test anonymous IIFE
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: 'Error: Whoops!\n    at repl:1:21\n'
  }
];

tests.forEach(run);
