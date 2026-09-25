'use strict';

const common = require('../common');
const assert = require('assert');
const { readFile } = require('fs');
const {
  createHook,
  executionAsyncResource,
  AsyncResource,
} = require('async_hooks');

// Ignore any asyncIds created before our hook is active.
let firstSeenAsyncId = -1;
const idResMap = new Map();
// The AsyncResource below plus at least one FSREQCALLBACK from readFile()
// (a small file is read in a single request).
const numExpectedCalls = 2;

createHook({
  init: common.mustCallAtLeast(
    (asyncId, type, triggerId, resource) => {
      if (firstSeenAsyncId === -1) {
        firstSeenAsyncId = asyncId;
      }
      assert.ok(idResMap.get(asyncId) === undefined);
      idResMap.set(asyncId, resource);
    }, numExpectedCalls),
  before(asyncId) {
    if (asyncId >= firstSeenAsyncId) {
      beforeHook(asyncId);
    }
  },
  after(asyncId) {
    if (asyncId >= firstSeenAsyncId) {
      afterHook(asyncId);
    }
  },
}).enable();

const beforeHook = common.mustCallAtLeast(
  (asyncId) => {
    const res = idResMap.get(asyncId);
    assert.ok(res !== undefined);
    const execRes = executionAsyncResource();
    assert.ok(execRes === res, 'resource mismatch in before');
  }, numExpectedCalls);

const afterHook = common.mustCallAtLeast(
  (asyncId) => {
    const res = idResMap.get(asyncId);
    assert.ok(res !== undefined);
    const execRes = executionAsyncResource();
    assert.ok(execRes === res, 'resource mismatch in after');
  }, numExpectedCalls);

const res = new AsyncResource('TheResource');
const initRes = idResMap.get(res.asyncId());
assert.ok(initRes === res, 'resource mismatch in init');
res.runInAsyncScope(common.mustCall(() => {
  const execRes = executionAsyncResource();
  assert.ok(execRes === res, 'resource mismatch in cb');
}));

readFile(__filename, common.mustCall());
