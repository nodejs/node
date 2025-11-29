'use strict';
require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

let count = 0;

function run({ command, expected, useColors = false }) {
  const { replServer, output } = startNewREPLServer(
    {
      prompt: '',
      terminal: false,
      useColors,
    },
    {
      disableDomainErrorAssert: true
    },
  );

  replServer.write(`${command}\n`);

  if (typeof expected === 'string') {
    assert.strictEqual(output.accumulator, expected);
  } else {
    assert.match(output.accumulator, expected);
  }

  // Verify that the repl is still working as expected.
  output.accumulator = '';
  replServer.write('1 + 1\n');
  // eslint-disable-next-line no-control-regex
  assert.strictEqual(output.accumulator.replace(/\u001b\[[0-9]+m/g, ''), '2\n');
  replServer.close();
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
