'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('composed');
const callbackChannel = dc.callbackTracingChannel(channel);
const promiseChannel = dc.promiseTracingChannel(channel);

let callbackContext;
let promiseContext;
let resolveCallback;
const callbackComplete = new Promise((resolve) => {
  resolveCallback = resolve;
});

assert.strictEqual('callback' in channel, false);
assert.strictEqual('promise' in channel, false);
assert.strictEqual('sync' in channel, false);
assert.strictEqual('syncIterator' in channel, false);
assert.strictEqual('asyncIterator' in channel, false);

assert.strictEqual(callbackChannel.asyncStart, promiseChannel.asyncStart);
assert.strictEqual(callbackChannel.asyncEnd, promiseChannel.asyncEnd);
assert.strictEqual(callbackChannel.error, promiseChannel.error);
assert.strictEqual(callbackChannel.asyncStart.name, 'tracing:composed:asyncStart');
assert.strictEqual(promiseChannel.asyncEnd.name, 'tracing:composed:asyncEnd');

callbackChannel.subscribe({
  asyncStart: common.mustCall((context) => {
    assert.strictEqual(context.label, 'hybrid');
    if (context.callback) {
      callbackContext = context;
      assert.strictEqual(context.callback.result, 'callback-result');
      return;
    }

    if (context.promise) {
      promiseContext = context;
      return;
    }

    assert.fail('Unexpected asyncStart context');
  }, 2),
  asyncEnd: common.mustCall((context) => {
    if (context.callback) {
      assert.strictEqual(context.callback.result, 'callback-result');
      return;
    }

    if (context.promise) {
      assert.strictEqual(context.promise.result, 'promise-result');
      return;
    }

    assert.fail('Unexpected asyncEnd context');
  }, 2),
  error: common.mustNotCall(),
});

const wrapped = channel.wrap(function(callback) {
  if (typeof callback === 'function') {
    setImmediate(callback, null, 'callback-result');
  }
  return Promise.resolve('promise-result');
}, {
  context() {
    return {
      label: 'hybrid',
    };
  },
  wrapArgs(args, context) {
    if (typeof args[0] !== 'function') {
      return args;
    }

    return callbackChannel.wrapArgs(args, 0, {
      context: () => context,
    });
  },
  mapResult(result, context) {
    return promiseChannel.wrap(result, {
      context: () => context,
    });
  },
});

const wrappedResult = wrapped(common.mustCall((err, result) => {
  assert.strictEqual(err, null);
  assert.strictEqual(result, 'callback-result');
  resolveCallback();
}));

Promise.all([
  wrappedResult,
  callbackComplete,
]).then(common.mustCall(([result]) => {
  assert.strictEqual(result, 'promise-result');
  assert.strictEqual(callbackContext, promiseContext);
}));

const iteratorPromiseChannel = dc.promiseTracingChannel('iterator');
const iteratorChannel = dc.syncIteratorTracingChannel('iterator');

let settledContext;
let yieldedContext;

iteratorPromiseChannel.subscribe({
  asyncStart: common.mustCall((context) => {
    settledContext = context;
  }),
  asyncEnd: common.mustCall(),
  error: common.mustNotCall(),
});

iteratorChannel.subscribe({
  yield: common.mustCall((context) => {
    yieldedContext = context;
    assert.strictEqual(context.iterator.result.value, 1);
  }),
  return: common.mustCall((context) => {
    assert.strictEqual(context.iterator.result.value, 2);
    assert.strictEqual(context, settledContext);
  }),
  error: common.mustNotCall(),
});

function* values() {
  yield 1;
  return 2;
}

iteratorPromiseChannel.wrap(Promise.resolve(values()), {
  context() {
    return {
      label: 'iterator',
    };
  },
  mapResult(iterator, context) {
    return iteratorChannel.wrap(iterator, {
      context: () => context,
    });
  },
}).then(common.mustCall((iterator) => {
  assert.strictEqual(iterator.next().value, 1);
  assert.strictEqual(iterator.next().value, 2);
  assert.strictEqual(settledContext, yieldedContext);
}));
