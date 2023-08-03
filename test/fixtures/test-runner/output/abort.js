'use strict';
require('../../../common');
const test = require('node:test');

test('promise timeout signal', { signal: AbortSignal.timeout(1) }, async (t) => {
  await Promise.all([
    t.test('ok 1', async () => {}),
    t.test('ok 2', () => {}),
    t.test('ok 3', { signal: t.signal }, async () => {}),
    t.test('ok 4', { signal: t.signal }, () => {}),
    t.test('not ok 1', () => new Promise(() => {})),
    t.test('not ok 2', (t, done) => {}),
    t.test('not ok 3', { signal: t.signal }, () => new Promise(() => {})),
    t.test('not ok 4', { signal: t.signal }, (t, done) => {}),
    t.test('not ok 5', { signal: t.signal }, (t, done) => {
      t.signal.addEventListener('abort', done);
    }),
  ]);
});

test('promise abort signal', { signal: AbortSignal.abort() }, async (t) => {
  await t.test('should not appear', () => {});
});

test('callback timeout signal', { signal: AbortSignal.timeout(1) }, (t, done) => {
  t.test('ok 1', async () => {});
  t.test('ok 2', () => {});
  t.test('ok 3', { signal: t.signal }, async () => {});
  t.test('ok 4', { signal: t.signal }, () => {});
  t.test('not ok 1', () => new Promise(() => {}));
  t.test('not ok 2', (t, done) => {});
  t.test('not ok 3', { signal: t.signal }, () => new Promise(() => {}));
  t.test('not ok 4', { signal: t.signal }, (t, done) => {});
  t.test('not ok 5', { signal: t.signal }, (t, done) => {
    t.signal.addEventListener('abort', done);
  });
});

test('callback abort signal', { signal: AbortSignal.abort() }, (t, done) => {
  t.test('should not appear', done);
});

// AbortSignal.timeout(1) doesn't prevent process from closing
// thus we have to keep the process open to prevent cancelation
// of the entire test tree
setTimeout(() => {}, 1000);
