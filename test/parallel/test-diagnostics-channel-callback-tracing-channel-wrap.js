'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.callbackTracingChannel('test');

assert.strictEqual(channel.asyncStart.name, 'tracing:test:asyncStart');
assert.strictEqual(channel.asyncEnd.name, 'tracing:test:asyncEnd');
assert.strictEqual(channel.error.name, 'tracing:test:error');

const callbackThis = { name: 'callback-this' };
const result = { ok: true };
const args = [() => {}, 'value'];

const handlers = {
  asyncStart: common.mustCall((context) => {
    assert.strictEqual(context.callback.result, result);
    assert.strictEqual(context.kind, 'wrapped');
  }),
  asyncEnd: common.mustCall((context) => {
    assert.strictEqual(context.callback.result, result);
    assert.strictEqual(context.kind, 'wrapped');
  }),
  error: common.mustNotCall(),
};

channel.subscribe(handlers);

const wrapped = channel.wrap(common.mustCall(function(err, value) {
  assert.strictEqual(this, callbackThis);
  assert.strictEqual(err, null);
  assert.strictEqual(value, result);
}), {
  context: { kind: 'wrapped' },
});

wrapped.call(callbackThis, null, result);

const wrappedArgs = channel.wrapArgs(args, 0, {
  context(first, second) {
    return {
      first,
      second,
    };
  },
});

assert.notStrictEqual(wrappedArgs, args);
assert.strictEqual(wrappedArgs[1], 'value');

channel.unsubscribe(handlers);

const wrappedArgsChannel = dc.callbackTracingChannel('args');

wrappedArgsChannel.subscribe({
  asyncStart: common.mustCall((context) => {
    assert.strictEqual(context.first, 'first');
    assert.strictEqual(context.second, 'second');
    assert.strictEqual(context.callback.error, 'first');
    assert.strictEqual(context.callback.result, 'second');
  }),
  asyncEnd: common.mustCall((context) => {
    assert.strictEqual(context.first, 'first');
    assert.strictEqual(context.second, 'second');
    assert.strictEqual(context.callback.error, 'first');
    assert.strictEqual(context.callback.result, 'second');
  }),
  error: common.mustCall((context) => {
    assert.strictEqual(context.callback.error, 'first');
  }),
});

const mapped = dc.callbackTracingChannel('mapped');

mapped.subscribe({
  asyncStart: common.mustCall((context) => {
    assert.strictEqual(context.callback.result, 'second');
    assert.strictEqual(context.callback.error, 'first');
  }),
  asyncEnd: common.mustCall((context) => {
    assert.strictEqual(context.callback.result, 'second');
    assert.strictEqual(context.callback.error, 'first');
  }),
  error: common.mustCall((context) => {
    assert.strictEqual(context.callback.error, 'first');
  }),
});

const argsCallback = wrappedArgsChannel.wrapArgs(args, 0, {
  context: common.mustCall(function(first, second) {
    assert.strictEqual(this, callbackThis);
    return {
      first,
      second,
    };
  }),
  mapOutcome(first, second) {
    return {
      error: first,
      result: second,
    };
  },
});

argsCallback[0].call(callbackThis, 'first', 'second');

const mappedCallback = mapped.wrap(common.mustCall(), {
  mapOutcome(first, second) {
    return {
      error: first,
      result: second,
    };
  },
});

mappedCallback('first', 'second');
