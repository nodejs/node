import '../common/index.mjs';
import assert from 'node:assert';
import fixtures from '../common/fixtures.js';
import { startNewREPLServer } from '../common/repl.js';

// Normalize line/column numbers (and the inspector-assigned REPL source
// number, which depends on how many evaluations have run in the process).
const stackRegExp = /(REPL)\d+:[0-9]+:[0-9]+/g;

async function runTest({ command, expected }) {
  const { replServer, output, run } = startNewREPLServer({
    terminal: false,
    useColors: false
  });

  await run(`${command}\n`);
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

// The REPL now evaluates asynchronously through the V8 inspector and surfaces
// the live Error object. Reading its `.stack` runs the custom
// `Error.prepareStackTrace` over the full V8 stack, so after the user's REPL
// frames the inspector/eval internals also appear. The custom formatter is
// still honored (frames joined with `--->`, error pushed to the front), which
// is what these tests verify; we match the user-visible prefix and tolerate
// the trailing internal frames.
const tests = [
  {
    // test .load for a file that throws
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: /^\| Uncaught Error: Whoops!--->\nREPL:\*:\*--->\nd \(REPL:\*:\*\)/m
  },
  {
    command: 'let x y;',
    expected: /^Uncaught SyntaxError: Unexpected identifier.*\n/
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: /^Uncaught Error: Whoops!--->\nREPL:\*:\*/
  },
  {
    command: 'foo = bar;',
    expected: /^Uncaught ReferenceError: bar is not defined--->\nREPL:\*:\*/
  },
  // test anonymous IIFE
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: /^Uncaught Error: Whoops!--->\nREPL:\*:\*--->\nREPL:\*:\*/
  },
];

for (const test of tests) {
  await runTest(test);
}

// Verify that the stack can be generated when Error.prepareStackTrace is
// deleted. The default formatter is restored, so the live error prints its
// normal stack.
delete Error.prepareStackTrace;
await runTest({
  command: 'throw new TypeError(\'Whoops!\')',
  expected: /^Uncaught TypeError: Whoops!\n {4}at REPL:\*:\*/
});
