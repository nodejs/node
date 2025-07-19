'use strict';

const common = require('../common');
const repl = require('repl');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');

const completionTests = [
  { send: 'Promise.reject().' },
  { send: 'let p = Promise.reject().' },
  { send: 'Promise.resolve().' },
  { send: 'Promise.resolve().then(() => {}).' },
  { send: `async function f() {throw new Error('test');}; f().` },
  { send: `async function f() {}; f().` },
];

(async function() {
  await runReplCompleteTests(completionTests);
})().then(common.mustCall());

async function runReplCompleteTests(tests) {
  const input = new ArrayStream();
  const output = new ArrayStream();

  const replServer = repl.start({
    prompt: '',
    input,
    output: output,
    allowBlockingCompletions: true,
    terminal: true
  });

  replServer._domain.on('error', (e) => {
    assert.fail(`Error in REPL domain: ${e}`);
  });
  for (const { send } of tests) {
    replServer.complete(
      send,
      common.mustCall((error, data) => {
        assert.strictEqual(error, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(typeof data[1], 'string');
        assert.ok(send.includes(data[1]));
      })
    );
  }
}
