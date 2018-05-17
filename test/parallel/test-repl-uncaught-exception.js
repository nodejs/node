'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

let count = 0;

function run({ command, expected }) {
  let accum = '';

  const output = new ArrayStream();
  output.write = (data) => accum += data.replace('\r', '');

  const r = repl.start({
    prompt: '',
    input: new ArrayStream(),
    output,
    terminal: false,
    useColors: false
  });

  r.write(`${command}\n`);
  if (typeof expected === 'string') {
    assert.strictEqual(accum, expected);
  } else {
    assert(expected.test(accum), accum);
  }
  r.close();
  count++;
}

const tests = [
  {
    command: 'x',
    expected: 'Thrown:\n' +
              'ReferenceError: x is not defined\n'
  },
  {
    command: 'process.on("uncaughtException", () => console.log("Foobar"));\n',
    expected: /^Thrown:\nTypeError \[ERR_INVALID_REPL_INPUT]: Unhandled exception/
  },
  {
    command: 'x;\n',
    expected: 'Thrown:\n' +
              'ReferenceError: x is not defined\n'
  },
  {
    command: 'process.on("uncaughtException", () => console.log("Foobar"));' +
             'console.log("Baz");\n',
    // eslint-disable-next-line node-core/no-unescaped-regexp-dot
    expected: /^Baz(.|\n)*ERR_INVALID_REPL_INPUT/
  }
];

process.on('exit', () => {
  assert.strictEqual(count, tests.length);
});

tests.forEach(run);
