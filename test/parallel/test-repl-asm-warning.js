'use strict';
const common = require('../common');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

// Regression test for https://github.com/nodejs/node/issues/63473
// V8 warnings emitted during REPL inspector preview inside
// DisallowJavascriptExecutionScope should not crash the process.

const { replServer, input } = startNewREPLServer({
  terminal: true,
  preview: true,
});

try {
  input.run([
    'function AsmModule() {',
    "  'use asm';",
    '  function add(a, b) {',
    '    a = a | 0;',
    '    b = b | 0;',
    '    return a + b;',
    '  }',
    '}',
    'AsmModule();',
  ]);
} finally {
  replServer.close();
}
