'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const repl = require('repl');


function run({ command, expected, ...extraREPLOptions }) {
  let accum = '';

  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();

  outputStream.write = (data) => accum += data.replace('\r', '');

  const r = repl.start({
    prompt: '',
    input: inputStream,
    output: outputStream,
    terminal: false,
    useColors: false,
    ...extraREPLOptions
  });

  r.write(`${command}\n`);
  assert.strictEqual(accum, expected);
  r.close();
}

const tests = [
  {
    // Test .load for a file that throws.
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: 'Thrown:\nError: Whoops!\n    at repl:9:24\n' +
              '    at d (repl:12:3)\n    at c (repl:9:3)\n' +
              '    at b (repl:6:3)\n    at a (repl:3:3)\n'
  },
  {
    command: 'let x y;',
    expected: 'Thrown:\n' +
              'let x y;\n      ^\n\nSyntaxError: Unexpected identifier\n'
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Thrown:\nError: Whoops!\n'
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: "Thrown:\nError: Whoops!\n    at repl:1:22 {\n  foo: 'bar'\n}\n",
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: 'Thrown:\nError: Whoops!\n    at repl:1:22 {\n  foo: ' +
              "\u001b[32m'bar'\u001b[39m\n}\n",
    useColors: true
  },
  {
    command: 'foo = bar;',
    expected: 'Thrown:\nReferenceError: bar is not defined\n'
  },
  // Test anonymous IIFE.
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: 'Thrown:\nError: Whoops!\n    at repl:1:21\n'
  }
];

tests.forEach(run);
