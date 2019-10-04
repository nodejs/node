// Flags: --experimental-modules
import '../common/index.mjs';
import assert from 'assert';
import { createHook } from 'async_hooks';

const bootstrapCalls = {};
const calls = {};

const asyncHook = createHook({ init, before, after, destroy, promiseResolve });
asyncHook.enable();

function init(asyncId, type, asyncTriggerId, resource, bootstrap) {
  (bootstrap ? bootstrapCalls : calls)[asyncId] = {
    init: true
  };
}

function before(asyncId) {
  (bootstrapCalls[asyncId] || calls[asyncId]).before = true;
}

function after(asyncId) {
  (bootstrapCalls[asyncId] || calls[asyncId]).after = true;
}

function destroy(asyncId) {
  (bootstrapCalls[asyncId] || calls[asyncId]).destroy = true;
}

function promiseResolve(asyncId) {
  (bootstrapCalls[asyncId] || calls[asyncId]).promiseResolve = true;
}

// Ensure all hooks have inits
assert(Object.values(bootstrapCalls).every(({ init }) => init === true));
assert(Object.values(calls).every(({ init }) => init === true));

// Ensure we have at least one of each type of callback
assert(Object.values(bootstrapCalls).some(({ before }) => before === true));
assert(Object.values(bootstrapCalls).some(({ after }) => after === true));
assert(Object.values(bootstrapCalls).some(({ destroy }) => destroy === true));

// Wait a tick to see that we have promise resolution
setTimeout(() => {
  assert(Object.values(bootstrapCalls).some(({ promiseResolve }) =>
    promiseResolve === true));
}, 10);
