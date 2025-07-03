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
  { send: 'const foo = { bar: Promise.reject() }; foo.bar.' },
  // Test for that reclusiveCatchPromise does not infinitely recurse
  // see lib/repl.js:reclusiveCatchPromise
  { send: 'const a = {}; a.self = a; a.self.' },
  { run: `const foo = { get name() { return Promise.reject(); } };`,
    send: `foo.name` },
  { run: 'const baz = { get bar() { return ""; } }; const getPropText = () => Promise.reject();',
    send: 'baz[getPropText()].',
    completeError: true },
  {
    send: 'const quux = { bar: { return Promise.reject(); } }; const getPropText = () => "bar"; quux[getPropText()].',
  },
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

  for (const { send, run, completeError = false } of tests) {
    if (run) {
      await new Promise((resolve, reject) => {
        replServer.eval(run, replServer.context, '', (err) => {
          if (err) {
            reject(err);
          } else {
            resolve();
          }
        });
      });
    }

    const onError = (e) => {
      assert.fail(`Unexpected error: ${e.message}`);
    };

    let completeErrorPromise = Promise.resolve();

    if (completeError) {
      completeErrorPromise = new Promise((resolve) => {
        const handleError = () => {
          replServer._completeDomain.removeListener('error', handleError);
          resolve();
        };
        replServer._completeDomain.on('error', handleError);
      });
    } else {
      replServer._completeDomain.on('error', onError);
    }

    await replServer.complete(
      send,
      common.mustCall((error, data) => {
        assert.strictEqual(error, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(typeof data[1], 'string');
        assert.ok(send.includes(data[1]));
      })
    );

    await completeErrorPromise;

    if (!completeError) {
      await new Promise((resolve) => {
        setImmediate(() => {
          replServer._completeDomain.removeListener('error', onError);
          resolve();
        });
      });
    }
  }
}
