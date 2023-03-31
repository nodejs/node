'use strict';

const common = require('../common');
const { setTimeout } = require('node:timers/promises');
const { AsyncLocalStorage } = require('async_hooks');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const store = new AsyncLocalStorage();

const context = { foo: 'bar' };

channel.start.bindStore(store, common.mustCall(() => {
  return context;
}));

assert.strictEqual(store.getStore(), undefined);
channel.tracePromise(common.mustCall(async () => {
  assert.deepStrictEqual(store.getStore(), context);
  await setTimeout(1);
  assert.deepStrictEqual(store.getStore(), context);
}));
assert.strictEqual(store.getStore(), undefined);
