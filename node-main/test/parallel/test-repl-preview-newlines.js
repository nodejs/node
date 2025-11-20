'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

common.skipIfInspectorDisabled();

const { input, output } = startNewREPLServer(
  { useColors: true }, { disableDomainErrorAssert: true }
);

output.accumulator = '';

for (const char of ['\\n', '\\v', '\\r']) {
  input.emit('data', `"${char}"()`);
  // Make sure the output is on a single line
  assert.strictEqual(output.accumulator, `"${char}"()\n\x1B[90mTypeError: "\x1B[39m\x1B[7G\x1B[1A`);
  input.run(['']);
  output.accumulator = '';
}
