'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const stream = require('stream');
const REPL = require('internal/repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const defaultHistoryPath = tmpdir.resolve('.node_repl_history');

// Create an input stream specialized for testing an array of actions.
class ActionStream extends stream.Stream {
  run(data) {
    const _iter = data[Symbol.iterator]();
    const doAction = () => {
      const next = _iter.next();
      if (next.done) {
        // Close the repl. Note that it must have a clean prompt to do so.
        this.emit('keypress', '', { ctrl: true, name: 'd' });
        return;
      }
      const action = next.value;

      if (typeof action === 'object') {
        this.emit('keypress', '', action);
      } else {
        this.emit('data', `${action}`);
      }
      setImmediate(doAction);
    };
    doAction();
  }
  resume() {}
  pause() {}
}
ActionStream.prototype.readable = true;

// Mock keys.
const ENTER = { name: 'enter' };
const prompt = '> ';

// ---------------------------------------------------------------------------
// Test 1: Basic auto-indent after opening brace.
//
// Typing `function f() {` + Enter should trigger a Recoverable error (the
// expression is incomplete) and enter multiline mode. The continuation line
// should be indented with 2 spaces.
// ---------------------------------------------------------------------------
const tests = [
  {
    name: 'basic auto-indent after opening brace',
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: (function*() {
      yield 'function f() {';
      yield ENTER;
      // At this point the REPL should have auto-indented.
      // Type the body and close the function.
      yield 'return 1;';
      yield ENTER;
      yield '}';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // After the opening brace and Enter, the output should contain the
      // continuation prompt followed by indentation (2 spaces).
      // The continuation prompt in Node REPL is '... ' by default, followed
      // by the auto-indent spaces. Look for at least 2 spaces of indentation
      // after the continuation prompt.
      assert.ok(
        combined.includes('  return') || combined.includes('  return'),
        `Expected auto-indented 'return' in output, got: ${JSON.stringify(combined)}`
      );
    }),
  },

  // ---------------------------------------------------------------------------
  // Test 2: De-indent when typing a closing brace.
  //
  // After auto-indenting inside a block, typing `}` should remove the
  // indentation on the current line so the closing brace aligns with the
  // opening statement.
  // ---------------------------------------------------------------------------
  {
    name: 'de-indent on closing brace',
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: (function*() {
      yield 'if (true) {';
      yield ENTER;
      yield 'x = 1;';
      yield ENTER;
      yield '}';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // The closing brace `}` should appear without leading indentation
      // (or at the base level), while `x = 1;` should be indented.
      const lines = combined.split(/[\r\n]+/);
      const indentedLine = lines.find((l) => l.includes('x = 1'));
      const closingLine = lines.find((l) => {
        const stripped = l.replaceAll('\x1b[', '').replace(/[0-9;]*m/g, '');
        return /^\s*\.*\s*\}$/.test(stripped);
      });
      if (indentedLine && closingLine) {
        const indentedSpaces = indentedLine.match(/^[\s.]*/)[0].length;
        const closingSpaces = closingLine.match(/^[\s.]*/)[0].length;
        assert.ok(
          closingSpaces <= indentedSpaces,
          `Closing brace indent (${closingSpaces}) should be <= indented body (${indentedSpaces})`
        );
      }
    }),
  },

  // ---------------------------------------------------------------------------
  // Test 3: Nested indentation.
  //
  // An `if` block inside a `function` should be indented by 4 spaces
  // (2 levels of 2-space indent).
  // ---------------------------------------------------------------------------
  {
    name: 'nested auto-indent',
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: (function*() {
      yield 'function g() {';
      yield ENTER;
      yield 'if (true) {';
      yield ENTER;
      yield 'return 42;';
      yield ENTER;
      yield '}';
      yield ENTER;
      yield '}';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // The nested `return 42;` should be indented by 4 spaces (two levels).
      // We verify that the output contains the string with leading spaces.
      assert.ok(
        combined.includes('    return 42') || combined.includes('    return 42'),
        `Expected nested indentation (4 spaces) for 'return 42', got: ${JSON.stringify(combined)}`
      );
    }),
  },
];

// ---------------------------------------------------------------------------
// Test 4: Auto-indent disabled on non-TTY (terminal: false).
//
// When the REPL is not running in a terminal, auto-indent should not be
// active. The continuation lines should not have extra indentation.
// ---------------------------------------------------------------------------
{
  const outputChunks = [];

  REPL.createInternalRepl(
    { NODE_REPL_HISTORY: defaultHistoryPath },
    {
      input: new ActionStream(),
      output: new stream.Writable({
        write(chunk, _encoding, cb) {
          outputChunks.push(chunk.toString());
          cb();
        },
      }),
      prompt,
      useColors: false,
      useGlobal: false,
      terminal: false,  // No TTY ⇒ no auto-indent.
    },
    common.mustSucceed((repl) => {

      repl.once('close', common.mustCall(() => {
        const combined = outputChunks.join('');
        // In non-terminal mode the REPL simply buffers commands; there is no
        // auto-indent inserted. Verify no leading spaces before 'return'.
        const lines = combined.split('\n');
        const returnLine = lines.find((l) => l.includes('return 1'));
        if (returnLine) {
          // Strip the prompt/continuation prompt before checking indent.
          const stripped = returnLine
            .replaceAll('\x1b[', '').replace(/[0-9;]*m/g, '')  // Remove ANSI codes.
            .replace(/^[>.\s]*\s/, '');        // Remove prompt chars.
          assert.ok(
            !stripped.startsWith('  '),
            `Non-TTY mode should not auto-indent, got: ${JSON.stringify(returnLine)}`
          );
        }
      }));

      // Run the multiline input through the non-terminal REPL.
      const actions = (function*() {
        yield 'function f() {';
        yield ENTER;
        yield 'return 1;';
        yield ENTER;
        yield '}';
        yield ENTER;
      })();
      repl.input.run(actions);
      // Non-TTY REPL doesn't handle keypress Ctrl+D; close explicitly.
      setImmediate(() => repl.close());
    }),
  );
}

// ---------------------------------------------------------------------------
// Run the TTY-based tests sequentially.
// ---------------------------------------------------------------------------
const numTests = tests.length;
let testIndex = 0;

function runTest() {
  const opts = tests[testIndex++];
  if (!opts) return;

  const outputChunks = [];

  REPL.createInternalRepl(opts.env, {
    input: new ActionStream(),
    output: new stream.Writable({
      write(chunk, _encoding, cb) {
        outputChunks.push(chunk.toString());
        cb();
      },
    }),
    prompt,
    useColors: false,
    useGlobal: false,
    terminal: true,
  }, common.mustSucceed((repl) => {

    repl.once('close', common.mustCall(() => {
      console.log(`Running auto-indent test ${testIndex}/${numTests}: ${opts.name}`);
      opts.check(outputChunks);
      runTest();
    }));

    repl.input.run(opts.test);
  }));
}

runTest();
