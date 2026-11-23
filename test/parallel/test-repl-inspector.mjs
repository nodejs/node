import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

// This test verifies that the V8 inspector API is usable in the REPL.

const { replServer, run, output } = startNewREPLServer();

await run(['const myVariable = 42']);

replServer.complete('myVar', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['myVariable'], 'myVar']);
}));

await run([
  'const inspector = require("inspector")',
  'const session = new inspector.Session()',
  'session.connect()',
  'session.post("Runtime.evaluate", { expression: "1 + 1" }, console.log)',
  'session.disconnect()',
]);

assert(output.accumulator.includes(
  "null { result: { type: 'number', value: 2, description: '2' } }"));

// Ensures that we do not rely on user-mutable global identifiers:
await run(['delete global, delete globalThis, delete Function, delete eval, delete Object, delete Reflect; Math']);

assert.match(output.accumulator, /^Object \[Math\] \{\}$/m);
