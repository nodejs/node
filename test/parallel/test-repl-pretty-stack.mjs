import '../common/index.mjs';
import assert from 'node:assert';
import fixtures from '../common/fixtures.js';
import { startNewREPLServer } from '../common/repl.js';

// The REPL now evaluates asynchronously through the V8 inspector. Errors are
// surfaced as the live Error object, so stack frames are printed with a
// `REPL<n>` source id whose number depends on how many evaluations have run in
// the process. Normalize both the line/column and the REPL id so the assertion
// only checks the shape of the stack, not the global evaluation counter.
const stackRegExp = /(at .*REPL)\d+:[0-9]+:[0-9]+/g;

async function runTest({ command, expected, ...extraREPLOptions }) {
  const { replServer, output, run } = startNewREPLServer({
    terminal: false,
    useColors: false,
    ...extraREPLOptions
  });

  await run(`${command}\n`);
  if (typeof expected === 'string') {
    assert.strictEqual(
      output.accumulator.replace(stackRegExp, '$1*:*:*'),
      expected.replace(stackRegExp, '$1*:*:*')
    );
  } else {
    assert.match(
      output.accumulator.replace(stackRegExp, '$1*:*:*'),
      expected
    );
  }
  replServer.close();
}

const tests = [
  {
    // Test .load for a file that throws. The whole file is evaluated as a
    // single inspector eval, so every frame shares the same REPL source id.
    // The loaded source is echoed with a `| ` prefix and the inspector surfaces
    // the innermost (anonymous callback) frame and the top-level call frame in
    // addition to the a/b/c/d frames.
    command: `.load ${fixtures.path('repl-pretty-stack.js')}`,
    expected: '| Uncaught Error: Whoops!\n    at REPL*:*:*\n' +
              '    at d (REPL*:*:*)\n    at c (REPL*:*:*)\n' +
              '    at b (REPL*:*:*)\n    at a (REPL*:*:*)\n' +
              '    at REPL*:*:*\n'
  },
  {
    // The async evaluator reports syntax errors without echoing the source and
    // caret that the synchronous vm path used to print.
    command: 'let x y;',
    expected: /^Uncaught SyntaxError: Unexpected identifier.*\n/
  },
  {
    command: 'throw new Error(\'Whoops!\')',
    expected: 'Uncaught Error: Whoops!\n    at REPL*:*:*\n'
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: 'Uncaught Error: Whoops!\n    at REPL*:*:*\n' +
              "    at REPL*:*:* {\n  foo: 'bar'\n}\n",
  },
  {
    command: '(() => { const err = Error(\'Whoops!\'); ' +
             'err.foo = \'bar\'; throw err; })()',
    expected: 'Uncaught Error: Whoops!\n    at REPL*:*:*\n' +
              '    at REPL*:*:* {\n  foo: ' +
              "[32m'bar'[39m\n}\n",
    useColors: true
  },
  {
    command: 'foo = bar;',
    expected: 'Uncaught ReferenceError: bar is not defined\n    at REPL*:*:*\n'
  },
  // Test anonymous IIFE.
  {
    command: '(function() { throw new Error(\'Whoops!\'); })()',
    expected: 'Uncaught Error: Whoops!\n    at REPL*:*:*\n    at REPL*:*:*\n'
  },
];

for (const test of tests) {
  await runTest(test);
}
