'use strict';
// Regression test for https://github.com/nodejs/node/issues/63551:
// Typing a bare `import` keyword in the REPL used to terminate the process.
//
// V8 throws SyntaxError("Cannot use import statement outside a module"),
// which the REPL enriched with a dynamic-import suggestion via
// `toDynamicImport`. That helper called acorn without a try/catch; on
// incomplete input acorn threw, and the exception escaped the REPL's input
// pipeline to crash the process.
//
// The fix wraps the acorn call in try/catch and falls back to a plain
// error message when the line cannot be parsed as a complete import
// statement. This test asserts that:
//   1. Emitting an incomplete `import` line through the REPL does not
//      throw out of the line-handler, and
//   2. The REPL still surfaces a SyntaxError to the user.

require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer, output } = startNewREPLServer();

replServer.emit('line', 'import');
replServer.emit('line', '.exit');

assert.match(output.accumulator, /SyntaxError/);
assert.match(
  output.accumulator,
  /Cannot use import statement inside the Node\.js REPL\b/,
);
