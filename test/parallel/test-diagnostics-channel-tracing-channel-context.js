'use strict';

const common = require('../common');

// This test ensures that tracing channels create an independent mutable
// context for each call while preserving explicitly provided contexts.

const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

const lengthChannel = dc.tracingChannel('test:length');
assert.strictEqual(lengthChannel.traceSync.length, 1);
assert.strictEqual(lengthChannel.tracePromise.length, 1);

function subscribe(channel, contexts, isPromise = false) {
  let endCalls = 0;
  let asyncStartCalls = 0;
  let asyncEndCalls = 0;

  channel.subscribe({
    start: common.mustCall((context) => {
      context.invocation = contexts.length;
      contexts.push(context);
    }, 4),
    end: common.mustCall((context) => {
      assert.strictEqual(context.invocation, endCalls++);
    }, 4),
    asyncStart: common.mustCall((context) => {
      assert.strictEqual(context.invocation, asyncStartCalls++);
    }, isPromise ? 4 : 0),
    asyncEnd: common.mustCall((context) => {
      assert.strictEqual(context.invocation, asyncEndCalls++);
    }, isPromise ? 4 : 0),
  });
}

const syncChannel = dc.tracingChannel('test:sync-context');
const syncContexts = [];
subscribe(syncChannel, syncContexts);

const syncProvided = { provided: true };
assert.strictEqual(syncChannel.traceSync(() => 'omitted'), 'omitted');
assert.strictEqual(syncChannel.traceSync(() => 'undefined', undefined),
                   'undefined');
assert.strictEqual(syncChannel.traceSync(() => 'provided', syncProvided),
                   'provided');

assert.strictEqual(Object.getPrototypeOf(syncContexts[0]), null);
assert.strictEqual(Object.getPrototypeOf(syncContexts[1]), null);
assert.notStrictEqual(syncContexts[0], syncContexts[1]);
assert.strictEqual(syncContexts[2], syncProvided);
assert.deepStrictEqual(syncContexts.slice(0, 3).map(({ result }) => result),
                       ['omitted', 'undefined', 'provided']);

const syncError = new Error('sync');
assert.throws(() => syncChannel.traceSync(() => {
  throw syncError;
}), (error) => error === syncError);
assert.strictEqual(syncContexts[3].error, syncError);

const promiseChannel = dc.tracingChannel('test:promise-context');
const promiseContexts = [];
subscribe(promiseChannel, promiseContexts, true);

const promiseProvided = { provided: true };
const promiseError = new Error('promise');
Promise.resolve()
  .then(() => promiseChannel.tracePromise(() => Promise.resolve('omitted')))
  .then(() => promiseChannel.tracePromise(
    () => Promise.resolve('undefined'), undefined))
  .then(() => promiseChannel.tracePromise(
    () => Promise.resolve('provided'), promiseProvided))
  .then(() => assert.rejects(
    promiseChannel.tracePromise(() => Promise.reject(promiseError)),
    promiseError))
  .then(common.mustCall(() => {
    assert.strictEqual(Object.getPrototypeOf(promiseContexts[0]), null);
    assert.strictEqual(Object.getPrototypeOf(promiseContexts[1]), null);
    assert.notStrictEqual(promiseContexts[0], promiseContexts[1]);
    assert.strictEqual(promiseContexts[2], promiseProvided);
    assert.deepStrictEqual(
      promiseContexts.slice(0, 3).map(({ result }) => result),
      ['omitted', 'undefined', 'provided']);
    assert.strictEqual(promiseContexts[3].error, promiseError);
  }));
