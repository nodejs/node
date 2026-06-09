'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.syncTracingChannel('test');

assert.strictEqual(channel.start.name, 'tracing:test:start');
assert.strictEqual(channel.end.name, 'tracing:test:end');
assert.strictEqual(channel.error.name, 'tracing:test:error');

const sharedContext = { type: 'seed' };
const thisArg = { name: 'receiver' };
const contexts = [];

channel.subscribe({
  start: common.mustCall((context) => {
    contexts.push(context);
  }, 3),
  end: common.mustCall((context) => {
    if (context.type === 'seed') {
      assert.strictEqual(context.call.result, 'ok');
      return;
    }

    assert.strictEqual(context.call.result, context.value + 1);
  }, 3),
  error: common.mustNotCall(),
});

const wrapped = channel.wrap(common.mustCall(function(value) {
  assert.strictEqual(this, thisArg);
  return value + 1;
}, 2), {
  context: common.mustCall(function(value) {
    assert.strictEqual(this, thisArg);
    return { value };
  }, 2),
});

assert.strictEqual(wrapped.call(thisArg, 1), 2);
assert.strictEqual(wrapped.call(thisArg, 2), 3);

assert.notStrictEqual(contexts[0], contexts[1]);
assert.deepStrictEqual(sharedContext, { type: 'seed' });

const seeded = channel.wrap(common.mustCall(() => 'ok'), {
  context: sharedContext,
});

assert.strictEqual(seeded(), 'ok');
assert.strictEqual(contexts[2], sharedContext);
