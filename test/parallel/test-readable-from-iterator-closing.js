'use strict';

const { mustCall, mustNotCall } = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

async function asyncSupport() {
  const finallyMustCall = mustCall();
  const bodyMustCall = mustCall();

  async function* infiniteGenerate() {
    try {
      while (true) yield 'a';
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(infiniteGenerate());

  for await (const chunk of stream) {
    bodyMustCall();
    assert.strictEqual(chunk, 'a');
    break;
  }
}

async function syncSupport() {
  const finallyMustCall = mustCall();
  const bodyMustCall = mustCall();

  function* infiniteGenerate() {
    try {
      while (true) yield 'a';
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(infiniteGenerate());

  for await (const chunk of stream) {
    bodyMustCall();
    assert.strictEqual(chunk, 'a');
    break;
  }
}

async function syncPromiseSupport() {
  const returnMustBeAwaited = mustCall();
  const bodyMustCall = mustCall();

  function* infiniteGenerate() {
    try {
      while (true) yield Promise.resolve('a');
    } finally {
      // eslint-disable-next-line no-unsafe-finally
      return { then(cb) {
        returnMustBeAwaited();
        cb();
      } };
    }
  }

  const stream = Readable.from(infiniteGenerate());

  for await (const chunk of stream) {
    bodyMustCall();
    assert.strictEqual(chunk, 'a');
    break;
  }
}

async function syncRejectedSupport() {
  const returnMustBeAwaited = mustCall();
  const bodyMustNotCall = mustNotCall();
  const catchMustCall = mustCall();
  const secondNextMustNotCall = mustNotCall();

  function* generate() {
    try {
      yield Promise.reject('a');
      secondNextMustNotCall();
    } finally {
      // eslint-disable-next-line no-unsafe-finally
      return { then(cb) {
        returnMustBeAwaited();
        cb();
      } };
    }
  }

  const stream = Readable.from(generate());

  try {
    for await (const chunk of stream) {
      bodyMustNotCall(chunk);
    }
  } catch {
    catchMustCall();
  }
}

async function syncRejectedAfterResolvedSupport() {
  const expectedError = new Error('later rejection');
  const finallyMustCall = mustCall();
  const bodyMustCall = mustCall((chunk) => {
    assert.strictEqual(chunk, 'a');
  });
  const catchMustCall = mustCall((error) => {
    assert.strictEqual(error, expectedError);
  });
  const thirdNextMustNotCall = mustNotCall();

  function* generate() {
    try {
      yield Promise.resolve('a');
      yield Promise.reject(expectedError);
      thirdNextMustNotCall();
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  try {
    for await (const chunk of stream) {
      bodyMustCall(chunk);
    }
  } catch (error) {
    catchMustCall(error);
  }

  assert.strictEqual(stream.destroyed, true);
}

async function noReturnAfterThrow() {
  const returnMustNotCall = mustNotCall();
  const bodyMustNotCall = mustNotCall();
  const catchMustCall = mustCall();
  const nextMustCall = mustCall();

  const stream = Readable.from({
    [Symbol.asyncIterator]() { return this; },
    async next() {
      nextMustCall();
      throw new Error('a');
    },
    async return() {
      returnMustNotCall();
      return { done: true };
    },
  });

  try {
    for await (const chunk of stream) {
      bodyMustNotCall(chunk);
    }
  } catch {
    catchMustCall();
  }
}

async function closeStreamWhileNextIsPending() {
  const finallyMustCall = mustCall();

  let resolveDestroy;
  const destroyed =
    new Promise((resolve) => { resolveDestroy = mustCall(resolve); });
  let resolveYielded;
  const yielded =
    new Promise((resolve) => { resolveYielded = mustCall(resolve); });

  async function* infiniteGenerate() {
    try {
      while (true) {
        yield 'a';
        resolveYielded();
        await destroyed;
      }
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(infiniteGenerate());

  stream.on('data', mustCall((data) => {
    assert.strictEqual(data, 'a');
  }));

  yielded.then(() => {
    stream.destroy();
    resolveDestroy();
  }).then(mustCall());
}

async function closeAfterNullYielded() {
  const finallyMustCall = mustCall();

  function* generate() {
    try {
      yield 'a';
      yield 'a';
      yield 'a';
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  stream.on('data', mustCall((chunk) => {
    assert.strictEqual(chunk, 'a');
  }, 3));
}

Promise.all([
  asyncSupport(),
  syncSupport(),
  syncPromiseSupport(),
  syncRejectedSupport(),
  syncRejectedAfterResolvedSupport(),
  noReturnAfterThrow(),
  closeStreamWhileNextIsPending(),
  closeAfterNullYielded(),
]).then(mustCall());
