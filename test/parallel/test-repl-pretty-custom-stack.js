'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const stackRegExp = /(REPL\d+):[0-9]+:[0-9]+/g;

function run({ command, expected }) {
  const { replServer, output } = startNewREPLServer({
    terminal: false,
    useColors: false
  }, {
    disableDomainErrorAssert: true,
  });

  replServer.write(`${command}\n`);
  if (typeof expected === 'string') {
    assert.strictEqual(
      output.accumulator.replace(stackRegExp, '$1:*:*'),
      expected.replace(stackRegExp, '$1:*:*')
    );
  } else {
    assert.match(
      output.accumulator.replace(stackRegExp, '$1:*:*'),
      expected
    );
  }
  replServer.close();
}

const origPrepareStackTrace = Error.prepareStackTrace;
Error.prepareStackTrace = (err, stack) => {
  if (err instanceof SyntaxError)
    return err.toString();
  // Insert the error at the beginning of the stack
  stack.unshift(err);
  return stack.join('--->\n');
};

process.on('uncaughtException', (e) => {
  Error.prepareStackTrace = origPrepareStackTrace;
  throw e;
});

const tests = [
  {
    // test .load for a file that throws
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: 'Uncaught Error: Whoops!--->\nREPL1:*:*--->\nd (REPL1:*:*)' +
              '--->\nc (REPL1:*:*)--->\nb (REPL1:*:*)--->\na (REPL1:*:*)\n'
  },
  {
    command: 'let x y;',
    expected: /let x y;\n {6}\^\n\nUncaught SyntaxError: Unexpected identifier.*\n/
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Uncaught Error: Whoops!\n'
  },
  {
    command: 'foo = bar;',
    expected: 'Uncaught ReferenceError: bar is not defined\n'
  },
  // test anonymous IIFE
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: 'Uncaught Error: Whoops!--->\nREPL5:*:*\n'
  },
];

tests.forEach(run);

// Verify that the stack can be generated when Error.prepareStackTrace is deleted.
delete Error.prepareStackTrace;
run({
  command: 'throw new TypeError(\'Whoops!\')',
  expected: 'Uncaught TypeError: Whoops!\n'
});
