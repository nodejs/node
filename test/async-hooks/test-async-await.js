'use strict';
const common = require('../common');

// This test ensures async hooks are being properly called
// when using async-await mechanics. This involves:
// 1. Checking that all initialized promises are being resolved
// 2. Checking that for each 'before' corresponding hook 'after' hook is called

const assert = require('assert');
const initHooks = require('./init-hooks');

const util = require('util');

const sleep = util.promisify(setTimeout);
// Either 'inited' or 'resolved'
const promisesInitState = new Map();
// Either 'before' or 'after' AND asyncId must be present in the other map
const promisesExecutionState = new Map();

const hooks = initHooks({
  oninit,
  onbefore,
  onafter,
  ondestroy: null,  // Intentionally not tested, since it will be removed soon
  onpromiseResolve,
});
hooks.enable();

function oninit(asyncId, type) {
  if (type === 'PROMISE') {
    promisesInitState.set(asyncId, 'inited');
  }
}

function onbefore(asyncId) {
  if (!promisesInitState.has(asyncId)) {
    return;
  }
  promisesExecutionState.set(asyncId, 'before');
}

function onafter(asyncId) {
  if (!promisesInitState.has(asyncId)) {
    return;
  }

  assert.strictEqual(promisesExecutionState.get(asyncId), 'before',
                     'after hook called for promise without prior call' +
                     'to before hook');
  assert.strictEqual(promisesInitState.get(asyncId), 'resolved',
                     'after hook called for promise without prior call' +
                     'to resolve hook');
  promisesExecutionState.set(asyncId, 'after');
}

function onpromiseResolve(asyncId) {
  assert(promisesInitState.has(asyncId),
         'resolve hook called for promise without prior call to init hook');

  promisesInitState.set(asyncId, 'resolved');
}

const timeout = common.platformTimeout(10);

function checkPromisesInitState() {
  for (const initState of promisesInitState.values()) {
    // Promise should not be initialized without being resolved.
    assert.strictEqual(initState, 'resolved');
  }
}

function checkPromisesExecutionState() {
  for (const executionState of promisesExecutionState.values()) {
    // Check for mismatch between before and after hook calls.
    assert.strictEqual(executionState, 'after');
  }
}

process.on('beforeExit', common.mustCall(() => {
  hooks.disable();
  hooks.sanityCheck('PROMISE');

  checkPromisesInitState();
  checkPromisesExecutionState();
}));

async function asyncFunc() {
  await sleep(timeout);
}

asyncFunc();
