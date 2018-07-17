'use strict';

const { disableCrashOnUnhandledRejection } = require('../common');
disableCrashOnUnhandledRejection();
process.on('unhandledRejection', () => {});

const assert = require('assert');

const util = require('util');

let calls = [];

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

const first = Promise.resolve('success');
let second;
const third = first.then(() => {
  second = Promise.reject('error');
  return second;
});


const check = (item, type, promise, value) => {
  assert.strictEqual(item.type, type);
  assert.strictEqual(item.promise, promise);
  assert.strictEqual(item.value, value);
};

setTimeout(() => {
  const fourth = third.catch(() => {});

  calls = calls.filter((c) =>
    [first, second, third, fourth].includes(c.promise));

  assert.strictEqual(calls.length, 8);

  check(calls[0], 'resolve', first, undefined);

  check(calls[1], 'resolve', second, undefined);

  check(calls[2], 'rejectWithNoHandler', second, 'error');

  check(calls[3], 'resolve', third, undefined);

  check(calls[4], 'handlerAddedAfterReject', second, undefined);

  check(calls[5], 'resolve', third, undefined);

  check(calls[6], 'rejectWithNoHandler', third, 'error');

  check(calls[7], 'handlerAddedAfterReject', third, undefined);
}, 10);
