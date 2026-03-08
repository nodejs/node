'use strict';

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');
const store = new AsyncLocalStorage();

const fnContext = { fn: 'context' };
const nextContext = { next: 'context' };

// Bind a store for the fn call (creates the generator object)
channel.start.bindStore(store, common.mustCall(() => fnContext));

// Bind a store for each next() call (runs the generator body)
nextChannel.start.bindStore(store, common.mustCall(() => nextContext));

assert.strictEqual(store.getStore(), undefined);

const iter = channel.traceIterator(common.mustCall(function*() {
  // Generator body runs during next(), inside nextChannel.start.runStores
  assert.deepStrictEqual(store.getStore(), nextContext);
  yield 1;
}), {});

assert.strictEqual(store.getStore(), undefined);
iter.next();
assert.strictEqual(store.getStore(), undefined);
