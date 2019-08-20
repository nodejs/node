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

  // Verify that the repl is still working as expected.
  accum = '';
  r.write('1 + 1\n');
  assert.strictEqual(accum, '2\n');
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
    expected: /^Thrown:\nTypeError \[ERR_INVALID_REPL_INPUT]: Listeners for `/
  },
  {
    command: 'x;\n',
    expected: 'Thrown:\n' +
              'ReferenceError: x is not defined\n'
  },
  {
    command: 'process.on("uncaughtException", () => console.log("Foobar"));' +
             'console.log("Baz");\n',
    expected: /^Thrown:\nTypeError \[ERR_INVALID_REPL_INPUT]: Listeners for `/
  },
  {
    command: 'console.log("Baz");' +
             'process.on("uncaughtException", () => console.log("Foobar"));\n',
    expected: /^Baz\nThrown:\nTypeError \[ERR_INVALID_REPL_INPUT]:.*uncaughtException/
  }
];

process.on('exit', () => {
  // To actually verify that the test passed we have to make sure no
  // `uncaughtException` listeners exist anymore.
  process.removeAllListeners('uncaughtException');
  assert.strictEqual(count, tests.length);
});

tests.forEach(run);
