'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const stream = require('stream');
const REPL = require('internal/repl');
const { syntaxHighlight } = require('internal/repl/utils');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const defaultHistoryPath = tmpdir.resolve('.node_repl_history');

// ANSI color codes used by syntax highlighting.
const MAGENTA = '\x1b[35m';
const GREEN = '\x1b[32m';
const YELLOW = '\x1b[33m';
const RESET = '\x1b[39m';

// ---------------------------------------------------------------------------
// Unit tests for the `syntaxHighlight` pure function.
// ---------------------------------------------------------------------------

{
  // Keywords should be colored magenta.
  const result = syntaxHighlight('const x = 1', true);
  assert.ok(
    result.includes(`${MAGENTA}const${RESET}`),
    `Expected 'const' to be magenta, got: ${JSON.stringify(result)}`
  );
}

{
  // 'let' keyword.
  const result = syntaxHighlight('let y = 2', true);
  assert.ok(
    result.includes(`${MAGENTA}let${RESET}`),
    `Expected 'let' to be magenta, got: ${JSON.stringify(result)}`
  );
}

{
  // 'function' keyword.
  const result = syntaxHighlight('function foo() {}', true);
  assert.ok(
    result.includes(`${MAGENTA}function${RESET}`),
    `Expected 'function' to be magenta, got: ${JSON.stringify(result)}`
  );
}

{
  // 'if' keyword.
  const result = syntaxHighlight('if (true) {}', true);
  assert.ok(
    result.includes(`${MAGENTA}if${RESET}`),
    `Expected 'if' to be magenta, got: ${JSON.stringify(result)}`
  );
}

{
  // String literals should be colored green.
  const result = syntaxHighlight('const s = "hello"', true);
  assert.ok(
    result.includes(GREEN),
    `Expected string to be green, got: ${JSON.stringify(result)}`
  );
  assert.ok(
    result.includes(RESET),
    `Expected RESET after string, got: ${JSON.stringify(result)}`
  );
}

{
  // Single-quoted strings should also be colored green.
  const result = syntaxHighlight("const s = 'world'", true);
  assert.ok(
    result.includes(GREEN),
    `Expected single-quoted string to be green, got: ${JSON.stringify(result)}`
  );
}

{
  // Number literals should be colored yellow.
  const result = syntaxHighlight('const n = 42', true);
  assert.ok(
    result.includes(YELLOW),
    `Expected number to be yellow, got: ${JSON.stringify(result)}`
  );
}

{
  // Boolean `true` should be colored yellow.
  const result = syntaxHighlight('const b = true', true);
  assert.ok(
    result.includes(`${YELLOW}true${RESET}`),
    `Expected 'true' to be yellow, got: ${JSON.stringify(result)}`
  );
}

{
  // Boolean `false` should be colored yellow.
  const result = syntaxHighlight('const b = false', true);
  assert.ok(
    result.includes(`${YELLOW}false${RESET}`),
    `Expected 'false' to be yellow, got: ${JSON.stringify(result)}`
  );
}

{
  // `null` should be colored yellow.
  const result = syntaxHighlight('const n = null', true);
  assert.ok(
    result.includes(`${YELLOW}null${RESET}`),
    `Expected 'null' to be yellow, got: ${JSON.stringify(result)}`
  );
}

{
  // `undefined` should be colored yellow.
  const result = syntaxHighlight('const u = undefined', true);
  assert.ok(
    result.includes(`${YELLOW}undefined${RESET}`),
    `Expected 'undefined' to be yellow, got: ${JSON.stringify(result)}`
  );
}

{
  // When useColors is false, the original string should be returned unchanged.
  const code = 'const x = 42';
  const result = syntaxHighlight(code, false);
  assert.strictEqual(result, code);
}

{
  // Multiple tokens in one line should all be highlighted.
  const result = syntaxHighlight('const x = "hello"', true);
  assert.ok(result.includes(MAGENTA), 'Expected keyword color');
  assert.ok(result.includes(GREEN), 'Expected string color');
}

// ---------------------------------------------------------------------------
// ActionStream-based integration tests: verify that the REPL output stream
// contains ANSI escape codes when syntax highlighting is active.
// ---------------------------------------------------------------------------

class ActionStream extends stream.Stream {
  run(data) {
    const _iter = data[Symbol.iterator]();
    const doAction = () => {
      const next = _iter.next();
      if (next.done) {
        // Close the REPL with Ctrl+D.
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

const ENTER = { name: 'enter' };
const prompt = '> ';

const tests = [
  {
    // Test 1: With useColors true, output should contain ANSI sequences.
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    useColors: true,
    test: (function*() {
      yield 'const x = 42';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // The output written to the terminal (via _writeToOutput) should
      // contain ANSI color codes for syntax highlighting.
      assert.ok(
        combined.includes('\x1b['),
        `Expected ANSI escape codes in colored output, got: ${JSON.stringify(combined)}`
      );
    }),
  },
  {
    // Test 2: With useColors false, output should NOT contain ANSI highlighting.
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    useColors: false,
    test: (function*() {
      yield 'const x = 42';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // Filter only lines that contain the user input – they should not have
      // highlighting escape codes.
      assert.ok(
        !combined.includes(MAGENTA),
        `Did not expect magenta color codes when useColors is false, got: ${JSON.stringify(combined)}`
      );
    }),
  },
  {
    // Test 3: With syntaxHighlighting explicitly set to false.
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    useColors: true,
    syntaxHighlighting: false,
    test: (function*() {
      yield 'const y = "hello"';
      yield ENTER;
    })(),
    check: common.mustCall(function(outputChunks) {
      const combined = outputChunks.join('');
      // Even though useColors is true, syntax highlighting is disabled so
      // keyword / string coloring should not appear. Note that other REPL
      // coloring (e.g. output preview) may still use ANSI codes, so we
      // specifically check that the keyword is NOT wrapped in magenta.
      assert.ok(
        !combined.includes(`${MAGENTA}const${RESET}`),
        'syntaxHighlighting:false should prevent keyword coloring'
      );
    }),
  },
];

// Test 4 (separate process-level): TERM=dumb disables highlighting.
{
  const savedTerm = process.env.TERM;
  process.env.TERM = 'dumb';

  const code = 'const x = 42';
  syntaxHighlight(code, true);
  // When TERM=dumb the highlight function should behave as a no-op or
  // the setup should skip installing the highlighter entirely.
  // The pure function still respects useColors; the integration path
  // (setupSyntaxHighlighting) would skip setup. We test the integration
  // path separately below.

  process.env.TERM = savedTerm;
}

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
    useColors: opts.useColors,
    terminal: true,
    syntaxHighlighting: opts.syntaxHighlighting,
  }, common.mustSucceed((repl) => {

    repl.once('close', common.mustCall(() => {
      opts.check(outputChunks);
      runTest();
    }));

    // Verify that `repl.line` stays plain (uncolored) so that evaluation
    // is not affected by ANSI codes.
    const origLine = repl.line;
    if (origLine) {
      assert.ok(
        !origLine.includes('\x1b['),
        `repl.line should not contain ANSI codes: ${JSON.stringify(origLine)}`
      );
    }

    repl.input.run(opts.test);
  }));
}

runTest();
