'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

let count = 0;

function run({ command, expected, useColors = false }) {
  let accum = '';

  const output = new ArrayStream();
  output.write = (data) => accum += data.replace('\r', '');

  const r = repl.start({
    prompt: '',
    input: new ArrayStream(),
    output,
    terminal: false,
    useColors
  });

  r.write(`${command}\n`);
  if (typeof expected === 'string') {
    assert.strictEqual(accum, expected);
  } else {
    assert.match(accum, expected);
  }

  // Verify that the repl is still working as expected.
  accum = '';
  r.write('1 + 1\n');
  // eslint-disable-next-line no-control-regex
  assert.strictEqual(accum.replace(/\u001b\[[0-9]+m/g, ''), '2\n');
  r.close();
  count++;
}

const tests = [
  {
    useColors: true,
    command: 'x',
    expected: 'Uncaught ReferenceError: x is not defined\n'
  },
  {
    useColors: true,
    command: 'throw { foo: "test" }',
    expected: "Uncaught { foo: \x1B[32m'test'\x1B[39m }\n"
  },
  {
    command: 'process.on("uncaughtException", () => console.log("Foobar"));\n',
    expected: /^Uncaught:\nTypeError \[ERR_INVALID_REPL_INPUT]: Listeners for `/
  },
  {
    command: 'x;\n',
    expected: 'Uncaught ReferenceError: x is not defined\n'
  },
  {
    command: 'process.on("uncaughtException", () => console.log("Foobar"));' +
             'console.log("Baz");\n',
    expected: /^Uncaught:\nTypeError \[ERR_INVALID_REPL_INPUT]: Listeners for `/
  },
  {
    command: 'console.log("Baz");' +
             'process.on("uncaughtException", () => console.log("Foobar"));\n',
    expected: /^Baz\nUncaught:\nTypeError \[ERR_INVALID_REPL_INPUT]:.*uncaughtException/
  },
];

process.on('exit', () => {
  // To actually verify that the test passed we have to make sure no
  // `uncaughtException` listeners exist anymore.
  process.removeAllListeners('uncaughtException');
  assert.strictEqual(count, tests.length);
});

tests.forEach(run);
