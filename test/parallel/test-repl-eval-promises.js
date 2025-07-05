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

const tests = [
  {
    send: 'Promise.reject()',
    expect: /Promise \{[\s\S]*?Uncaught undefined\n?$/
  },
  {
    send: 'let p = Promise.reject()',
    expect: /undefined\nUncaught undefined\n?$/
  },
  {
    send: `Promise.resolve()`,
    expect: /Promise \{[\s\S]*?}\n?$/
  },
  {
    send: `Promise.resolve().then(() => {})`,
    expect: /Promise \{[\s\S]*?}\n?$/
  },
  {
    send: `async function f() { throw new Error('test'); };f();`,
    expect: /Promise \{[\s\S]*?<rejected> Error: test[\s\S]*?Uncaught Error: test[\s\S]*?\n?$/
  },
  {
    send: `async function f() {};f();`,
    expect: /Promise \{[\s\S]*?}\n?$/
  },
];


(async function() {
  await runReplCompleteTests(completionTests);
  await runReplTests(tests);
})().then(common.mustCall());

async function runReplCompleteTests(tests) {
  let outputText = '';
  for (const { send } of tests) {
    const input = new ArrayStream();
    const output = new ArrayStream();

    function write(data) {
      outputText += data;
    }
    output.write = write;
    const replServer = repl.start({
      prompt: '',
      input,
      output: output,
      allowBlockingCompletions: true,
      terminal: true
    });

    input.emit('data', send);
    await new Promise((resolve) => {
      setTimeout(() => {
        assert.strictEqual(outputText.includes(send), true);
        assert.strictEqual(outputText.split(send).length - 1, 1);
        replServer.close();
        resolve();
      }, 10);
    });
  }
}

async function runReplTests(tests) {
  for (const { send, expect } of tests) {
    const input = new ArrayStream();
    const output = new ArrayStream();
    let outputText = '';
    function write(data) {
      outputText += data;
    }
    output.write = write;
    const replServer = repl.start({
      prompt: '',
      input,
      output: output,
    });
    input.emit('data', `${send}\n`);
    await new Promise((resolve) => {
      setTimeout(() => {
        assert.match(outputText, expect);
        replServer.close();
        resolve();
      }, 10);
    });
  }
}
