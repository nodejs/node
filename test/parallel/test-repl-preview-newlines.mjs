import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

common.skipIfInspectorDisabled();

// Ignore terminal settings so the preview remains active under TERM=dumb.
process.env.TERM = '';

// Keep syntax highlighting out of this test so it only covers preview layout.
// Preview and result colors are enabled after readline is initialized.
const { output, run, replServer } = startNewREPLServer({ useColors: false });
replServer.useColors = true;
replServer.writer.options.colors = true;

for (const char of ['\\n', '\\v', '\\r']) {
  output.accumulator = '';
  // Evaluation (and therefore the preview) is produced asynchronously.
  await run(`"${char}"()`);
  // Make sure the output is on a single line
  assert.strictEqual(output.accumulator, `"${char}"()\n\x1B[90mTypeError: "\x1B[39m\x1B[7G\x1B[1A`);
  await run(['']);
}

replServer.close();
