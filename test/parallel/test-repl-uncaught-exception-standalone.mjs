import '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

const { replServer, output, run } = startNewREPLServer();

await run('x\n');
await run(
  'process.on("uncaughtException", () => console.log("Foobar"));' +
  'console.log("short")\n');
await run('x\n');

assert.match(output.accumulator, /ReferenceError: x is not defined/);
assert.match(output.accumulator, /short/);
assert.match(output.accumulator, /Foobar/);

replServer.close();
