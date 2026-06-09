'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.promiseTracingChannel('test');
const callbackChannel = dc.callbackTracingChannel('test');

assert.strictEqual(channel.asyncStart.name, 'tracing:test:asyncStart');
assert.strictEqual(channel.asyncEnd.name, 'tracing:test:asyncEnd');
assert.strictEqual(channel.error.name, 'tracing:test:error');
assert.strictEqual(channel.asyncStart, callbackChannel.asyncStart);
assert.strictEqual(channel.asyncEnd, callbackChannel.asyncEnd);
assert.strictEqual(channel.error, callbackChannel.error);

const contexts = [];

channel.subscribe({
  asyncStart: common.mustCall((context) => {
    contexts.push(context);
    assert.strictEqual(context.promise.result, 2);
    assert.strictEqual(context.kind, 'promise');
  }),
  asyncEnd: common.mustCall((context) => {
    assert.strictEqual(context.promise.result, 2);
    assert.strictEqual(context.kind, 'promise');
  }),
  error: common.mustNotCall(),
});

channel.wrap(Promise.resolve(1), {
  context: { kind: 'promise' },
  mapResult(result) {
    return result + 1;
  },
}).then(common.mustCall((result) => {
  assert.strictEqual(result, 2);
  assert.strictEqual(contexts.length, 1);
}));

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(
    warning.message,
    "tracePromise was called with the function '<anonymous>', which returned a non-thenable.",
  );
}));

assert.strictEqual(channel.wrap(1), 1);
