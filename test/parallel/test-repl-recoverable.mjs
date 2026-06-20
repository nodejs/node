import '../common/index.mjs';
import assert from 'node:assert';
import repl from 'node:repl';
import { startNewREPLServer } from '../common/repl.js';

let evalCount = 0;

function customEval(code, context, file, cb) {
  evalCount++;

  return cb(evalCount === 1 ? new repl.Recoverable() : null, true);
}

const { output, run } = startNewREPLServer({ eval: customEval });

// https://github.com/nodejs/node/issues/2939
// Expose recoverable errors to the consumer.
await run('1\n');
await run('2\n');

assert(output.accumulator.includes('| '), 'REPL never recovered');
assert(output.accumulator.includes('true\n'), 'REPL never rendered the result');
assert.strictEqual(evalCount, 2);
