import * as common from '../common/index.mjs';
import { startNewREPLServer } from '../common/repl.js';
import { isMainThread } from 'node:worker_threads';
import assert from 'node:assert';

if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}

if (!isMainThread) {
  common.skip('No signal handling available in Workers');
}

const { run, output } = startNewREPLServer({ breakEvalOnSigint: true });

await run('a = 1001; process.kill(process.pid, "SIGINT"); while (true) {}\n');

const interrupted = 'Script execution was interrupted by `SIGINT`';
assert.ok(
  output.accumulator.includes(interrupted),
  `Expected output to contain "${interrupted}", got ${output.accumulator}`,
);

await run('a*2*3*7\n');
assert.ok(
  output.accumulator.includes('42042'),
  `Expected output to contain "42042", got ${output.accumulator}`,
);
