import '../common/index.mjs';
import assert from 'node:assert';
import { startNewREPLServer } from '../common/repl.js';

const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

const { replServer, output, run } = startNewREPLServer({ prompt: testingReplPrompt });

await run('throw new Error("foo[a]")\n');

// The REPL now evaluates via the inspector and surfaces the live Error object,
// so the printed stack includes a `at REPL<n>:line:col` frame after the
// message. Strip those frames before comparing the error header.
assert.strictEqual(
  output.accumulator
    .split('\n')
    .filter((line) => !line.includes(testingReplPrompt) && !/^\s+at /.test(line))
    .join(''),
  'Uncaught Error: foo[a]'
);

replServer.close();
