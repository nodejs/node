'use strict';

require('../common');
const assert = require('assert');
const util = require('util');

const calls = [];

const makeHook = (type) => (promise, value) => {
  calls.push({ type, promise, value });
};
util.createPromiseHook({
  resolve: makeHook('resolve'),
  rejectWithNoHandler: makeHook('rejectWithNoHandler'),
  handlerAddedAfterReject: makeHook('handlerAddedAfterReject'),
  rejectAfterResolved: makeHook('rejectAfterResolved'),
  resolveAfterResolved: makeHook('resolveAfterResolved'),
}).enable();

const x = (async () => {
  throw 'error'; // eslint-disable-line no-throw-literal
})();

const check = (item, type, value) => {
  assert.strictEqual(item.type, type);
  assert.strictEqual(item.value, value);
};

process.nextTick(() => {
  x.catch(() => {});

  assert.strictEqual(calls.length, 3);

  check(calls[0], 'resolve', undefined);
  check(calls[1], 'rejectWithNoHandler', 'error');
  check(calls[2], 'handlerAddedAfterReject', undefined);
});
