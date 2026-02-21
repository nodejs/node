'use strict';

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const store = new AsyncLocalStorage();

const firstContext = { foo: 'bar' };
const secondContext = { baz: 'buz' };

channel.start.bindStore(store, common.mustCall(() => {
  return firstContext;
}));

channel.asyncStart.bindStore(store, common.mustCall(() => {
  return secondContext;
}));

assert.strictEqual(store.getStore(), undefined);
channel.traceCallback(common.mustCall((cb) => {
  assert.deepStrictEqual(store.getStore(), firstContext);
  setImmediate(cb);
}), 0, {}, null, common.mustCall(() => {
  assert.deepStrictEqual(store.getStore(), secondContext);
}));
assert.strictEqual(store.getStore(), undefined);
