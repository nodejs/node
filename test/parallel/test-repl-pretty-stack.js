'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const repl = require('repl');

const stackRegExp = /(at .*REPL\d+:)[0-9]+:[0-9]+/g;

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
  if (typeof expected === 'string') {
    assert.strictEqual(
      accum.replace(stackRegExp, '$1*:*'),
      expected.replace(stackRegExp, '$1*:*')
    );
  } else {
    assert.match(
      accum.replace(stackRegExp, '$1*:*'),
      expected
    );
  }
  r.close();
}

const tests = [
  {
    // Test .load for a file that throws.
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: 'Uncaught Error: Whoops!\n    at REPL1:*:*\n' +
              '    at d (REPL1:*:*)\n    at c (REPL1:*:*)\n' +
              '    at b (REPL1:*:*)\n    at a (REPL1:*:*)\n'
  },
  {
    command: 'let x y;',
    expected: /^let x y;\n {6}\^\n\nUncaught SyntaxError: Unexpected identifier.*\n/
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Uncaught Error: Whoops!\n'
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: "Uncaught Error: Whoops!\n    at REPL4:*:* {\n  foo: 'bar'\n}\n",
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: 'Uncaught Error: Whoops!\n    at REPL5:*:* {\n  foo: ' +
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
    expected: 'Uncaught Error: Whoops!\n    at REPL7:*:*\n'
  },
];

tests.forEach(run);
