'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

// Test that --disallow-code-generation-from-strings can be passed to workers
// and properly blocks eval() and related code generation functions.

// Test 1: Worker with --disallow-code-generation-from-strings should block eval
{
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    try {
      eval('"test"');
      parentPort.postMessage({ evalBlocked: false });
    } catch (err) {
      parentPort.postMessage({ evalBlocked: true, errorName: err.name });
    }
  `, {
    eval: true,
    execArgv: ['--disallow-code-generation-from-strings']
  });

  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.evalBlocked, true);
    assert.strictEqual(msg.errorName, 'EvalError');
  }));

  worker.on('error', common.mustNotCall());
}

// Test 2: Worker without the flag should allow eval
{
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    try {
      const result = eval('"test"');
      parentPort.postMessage({ evalBlocked: false, result });
    } catch (err) {
      parentPort.postMessage({ evalBlocked: true });
    }
  `, {
    eval: true,
    execArgv: []
  });

  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.evalBlocked, false);
    assert.strictEqual(msg.result, 'test');
  }));

  worker.on('error', common.mustNotCall());
}

// Test 3: Verify the flag also blocks Function constructor
{
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    try {
      new Function('return 42')();
      parentPort.postMessage({ functionBlocked: false });
    } catch (err) {
      parentPort.postMessage({ functionBlocked: true, errorName: err.name });
    }
  `, {
    eval: true,
    execArgv: ['--disallow-code-generation-from-strings']
  });

  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.functionBlocked, true);
    assert.strictEqual(msg.errorName, 'EvalError');
  }));

  worker.on('error', common.mustNotCall());
}
