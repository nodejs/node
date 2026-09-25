'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const { finished } = require('stream/promises');

const expectedError = {
  code: 'ERR_STREAM_NULL_VALUES',
  name: 'TypeError',
  message: 'May not write null values to stream'
};

async function rejectsNull(iterable, expectedChunks = []) {
  const stream = Readable.from(iterable);
  const chunks = [];
  const completion = finished(stream);

  stream.on('data', (chunk) => chunks.push(chunk));

  await assert.rejects(completion, expectedError);
  assert.deepStrictEqual(chunks, expectedChunks);
  assert.strictEqual(stream.destroyed, true);
}

async function asyncIteratorYieldsNull() {
  const cleanup = common.mustCall();

  async function* generate() {
    try {
      yield null;
    } finally {
      cleanup();
    }
  }

  await rejectsNull(generate());
}

async function syncIteratorYieldsNull() {
  const cleanup = common.mustCall();

  function* generate() {
    try {
      yield null;
    } finally {
      cleanup();
    }
  }

  await rejectsNull(generate());
}

async function firstSyncValueResolvesToNull() {
  const cleanup = common.mustCall();

  function* generate() {
    try {
      yield Promise.resolve(null);
    } finally {
      cleanup();
    }
  }

  await rejectsNull(generate());
}

async function laterSyncValueResolvesToNull() {
  const cleanup = common.mustCall();

  function* generate() {
    try {
      yield Promise.resolve('first');
      yield Promise.resolve(null);
    } finally {
      cleanup();
    }
  }

  await rejectsNull(generate(), ['first']);
}

Promise.all([
  asyncIteratorYieldsNull(),
  syncIteratorYieldsNull(),
  firstSyncValueResolvesToNull(),
  laterSyncValueResolvesToNull(),
]).then(common.mustCall());
