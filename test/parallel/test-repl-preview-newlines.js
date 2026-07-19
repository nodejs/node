'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

// Ignore terminal settings so the preview remains active under TERM=dumb.
process.env.TERM = '';

// Keep syntax highlighting out of this test so it only covers preview layout.
// Preview and result colors are enabled after readline is initialized.
const { input, output, replServer } = startNewREPLServer({ useColors: false });
replServer.useColors = true;
replServer.writer.options.colors = true;

output.accumulator = '';

for (const char of ['\\n', '\\v', '\\r']) {
  input.emit('data', `"${char}"()`);
  // Make sure the output is on a single line
  assert.strictEqual(output.accumulator, `"${char}"()\n\x1B[90mTypeError: "\x1B[39m\x1B[7G\x1B[1A`);
  input.run(['']);
  output.accumulator = '';
}
