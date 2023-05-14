'use strict';
require('../common');

const {
  Readable,
} = require('stream');
const { it } = require('node:test');
const { strictEqual, deepStrictEqual } = require('assert');

const { from, unref } = Readable;

const nextTick = () => new Promise((resolve) => process.nextTick(resolve));

it('unref stream error should propagate to original one', async () => {
  const originalStream = from([1, 2, 3, 4, 5]);

  let emittedError;
  originalStream.on('error', (e) => {
    emittedError = e;
  });
  const unrefStream = unref(originalStream);

  const thrownError = new Error('test');

  unrefStream.destroy(thrownError);

  await nextTick();
  strictEqual(unrefStream.destroyed, true);
  strictEqual(originalStream.destroyed, true);

  // Need another tick to propagate the error
  await nextTick();
  strictEqual(emittedError, thrownError);
});

it('Original stream error should propagate to unref one', async () => {
  const originalStream = from([1, 2, 3, 4, 5]);
  const unrefStream = unref(originalStream);

  let emittedError;
  unrefStream.on('error', (e) => {
    emittedError = e;
  });

  const thrownError = new Error('test');

  originalStream.destroy(thrownError);

  await nextTick();
  strictEqual(unrefStream.destroyed, true);
  strictEqual(originalStream.destroyed, true);

  await nextTick();
  strictEqual(emittedError, thrownError);
});

it('Should not close original stream when unref one finished but not consumed all data', async () => {
  const originalStream = from([1, 2, 3, 4, 5]);

  const unrefStream = unref(originalStream);

  // eslint-disable-next-line no-unused-vars
  for await (const _ of unrefStream) {
    break;
  }

  await nextTick();
  strictEqual(unrefStream.destroyed, true);
  strictEqual(originalStream.destroyed, false);
});

it('Should close original stream when unref one consume all data', async () => {
  const originalStream = from([1, 2, 3, 4, 5]);

  const unrefStream = unref(originalStream);

  const data = await unrefStream.toArray();
  deepStrictEqual(data, [1, 2, 3, 4, 5]);

  await nextTick();

  strictEqual(unrefStream.destroyed, true);
  strictEqual(originalStream.destroyed, true);
});

it('original stream close should close unref one', async () => {
  const originalStream = from([1, 2, 3, 4, 5]);
  const unrefStream = unref(originalStream);

  await originalStream.toArray();

  strictEqual(originalStream.destroyed, true);
  strictEqual(unrefStream.destroyed, true);
});
