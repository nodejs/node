// Reference: https://github.com/nodejs/node/pull/7624
import '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

const { replServer, output, run } = startNewREPLServer({
  useColors: false,
  terminal: false
});

await run('function a() { return 42; } (1)\n');
await run('a\n');

const expected = '1\n[Function: a]\n';
const got = output.accumulator;
assert.strictEqual(got, expected);

replServer.close();
