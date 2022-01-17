'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');

function oneTo5() {
  return Readable.from([1, 2, 3, 4, 5]);
}

function oneTo5Async() {
  return oneTo5().map(async (x) => {
    await Promise.resolve();
    return x;
  });
}
{
  // Some and every work with a synchronous stream and predicate
  (async () => {
    assert.strictEqual(await oneTo5().some((x) => x > 3), true);
    assert.strictEqual(await oneTo5().every((x) => x > 3), false);
    assert.strictEqual(await oneTo5().some((x) => x > 6), false);
    assert.strictEqual(await oneTo5().every((x) => x < 6), true);
    assert.strictEqual(await Readable.from([]).some((x) => true), false);
    assert.strictEqual(await Readable.from([]).every((x) => true), true);
  })().then(common.mustCall());
}

{
  // Some and every work with an asynchronous stream and synchronous predicate
  (async () => {
    assert.strictEqual(await oneTo5Async().some((x) => x > 3), true);
    assert.strictEqual(await oneTo5Async().every((x) => x > 3), false);
    assert.strictEqual(await oneTo5Async().some((x) => x > 6), false);
    assert.strictEqual(await oneTo5Async().every((x) => x < 6), true);
  })().then(common.mustCall());
}

{
  // Some and every work on asynchronous streams with an asynchronous predicate
  (async () => {
    assert.strictEqual(await oneTo5().some(async (x) => x > 3), true);
    assert.strictEqual(await oneTo5().every(async (x) => x > 3), false);
    assert.strictEqual(await oneTo5().some(async (x) => x > 6), false);
    assert.strictEqual(await oneTo5().every(async (x) => x < 6), true);
  })().then(common.mustCall());
}

{
  // Some and every short circuit
  (async () => {
    await oneTo5().some(common.mustCall((x) => x > 2, 3));
    await oneTo5().every(common.mustCall((x) => x < 3, 3));
    // When short circuit isn't possible the whole stream is iterated
    await oneTo5().some(common.mustCall((x) => x > 6, 5));
    // The stream is destroyed afterwards
    const stream = oneTo5();
    await stream.some(common.mustCall((x) => x > 2, 3));
    assert.strictEqual(stream.destroyed, true);
  })().then(common.mustCall());
}

{
  // Support for AbortSignal
  const ac = new AbortController();
  assert.rejects(Readable.from([1, 2, 3]).some(
    () => new Promise(() => {}),
    { signal: ac.signal }
  ), {
    name: 'AbortError',
  }).then(common.mustCall());
  ac.abort();
}
{
  // Support for pre-aborted AbortSignal
  assert.rejects(Readable.from([1, 2, 3]).some(
    () => new Promise(() => {}),
    { signal: AbortSignal.abort() }
  ), {
    name: 'AbortError',
  }).then(common.mustCall());
}
{
  // Error cases
  assert.rejects(async () => {
    await Readable.from([1]).every(1);
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
  assert.rejects(async () => {
    await Readable.from([1]).every((x) => x, {
      concurrency: 'Foo'
    });
  }, /ERR_OUT_OF_RANGE/).then(common.mustCall());
}
