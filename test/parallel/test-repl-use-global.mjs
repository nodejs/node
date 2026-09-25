import '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

// Array of [useGlobal, expectedResult] pairs
const globalTestCases = [
  [false, 'undefined'],
  [true, '\'tacos\''],
  [undefined, 'undefined'],
];

// Test how the global object behaves in each state for useGlobal.
for (const [useGlobal, expected] of globalTestCases) {
  const { replServer, output, run } = startNewREPLServer({
    terminal: false,
    useColors: false,
    useGlobal,
  });

  globalThis.lunch = 'tacos';
  await run('globalThis.lunch;\n');
  replServer.close();
  delete globalThis.lunch;

  assert.strictEqual(output.accumulator.trim(), expected);
}

// Test how shadowing the process object via `let` behaves in each useGlobal
// state. Note: we can't actually test the state when useGlobal is true,
// because the exception that's generated is caught (see below), but errors
// are printed, and the test suite is aware of it, causing a failure to be
// flagged.
const processTestCases = [false, undefined];

for (const useGlobal of processTestCases) {
  const { replServer, output, run } = startNewREPLServer({
    terminal: false,
    useColors: false,
    useGlobal,
  });

  // If useGlobal is false, then `let process` should work.
  await run('let process;\n');
  await run('21 * 2;\n');
  replServer.close();

  assert.strictEqual(output.accumulator.trim(), 'undefined\n42');
}
