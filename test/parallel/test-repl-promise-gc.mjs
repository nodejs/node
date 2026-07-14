// Flags: --expose-gc

import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

const { output, replServer, run } = startNewREPLServer({
  terminal: false,
  useColors: false,
});

// The promise is not retained by user code. Force a collection while the REPL
// is awaiting it to verify that the inspector keeps it alive until settlement.
await run(`new Promise((resolve) => setImmediate(() => {
  globalThis.gc();
  resolve(42);
}))\n`);

assert.strictEqual(output.accumulator, '42\n');

replServer.close();
