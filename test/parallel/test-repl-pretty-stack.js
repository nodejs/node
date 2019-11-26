'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const repl = require('repl');

const stackRegExp = /(at .*repl:)[0-9]+:[0-9]+/g;

function run({ command, expected, ...extraREPLOptions }, i) {
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
  console.log(i);
  assert.strictEqual(
    accum.replace(stackRegExp, '$1*:*'),
    expected.replace(stackRegExp, '$1*:*')
  );
  r.close();
}

const tests = [
  {
    // Test .load for a file that throws.
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: 'Uncaught Error: Whoops!\n    at repl:*:*\n' +
              '    at d (repl:*:*)\n    at c (repl:*:*)\n' +
              '    at b (repl:*:*)\n    at a (repl:*:*)\n'
  },
  {
    command: 'let x y;',
    expected: 'let x y;\n      ^\n\n' +
              'Uncaught SyntaxError: Unexpected identifier\n'
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Uncaught Error: Whoops!\n'
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: "Uncaught Error: Whoops!\n    at repl:*:* {\n  foo: 'bar'\n}\n",
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: 'Uncaught Error: Whoops!\n    at repl:*:* {\n  foo: ' +
              "\u001b[32m'bar'\u001b[39m\n}\n",
    useColors: true
  },
  {
    command: 'foo = bar;',
    expected: 'Uncaught ReferenceError: bar is not defined\n'
  },
  // Test anonymous IIFE.
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: 'Uncaught Error: Whoops!\n    at repl:*:*\n'
  }
];

tests.forEach(run);
