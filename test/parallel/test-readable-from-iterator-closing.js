'use strict';

const { mustCall, mustNotCall } = require('../common');
const { Readable } = require('stream');
const { strictEqual } = require('assert');

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
    strictEqual(chunk, 'a');
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
    strictEqual(chunk, 'a');
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
    strictEqual(chunk, 'a');
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
  const dataMustCall = mustCall();

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

  stream.on('data', (data) => {
    dataMustCall();
    strictEqual(data, 'a');
  });

  yielded.then(() => {
    stream.destroy();
    resolveDestroy();
  });
}

async function closeAfterNullYielded() {
  const finallyMustCall = mustCall();
  const dataMustCall = mustCall(3);

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

  stream.on('data', (chunk) => {
    dataMustCall();
    strictEqual(chunk, 'a');
  });
}

Promise.all([
  asyncSupport(),
  syncSupport(),
  syncPromiseSupport(),
  syncRejectedSupport(),
  noReturnAfterThrow(),
  closeStreamWhileNextIsPending(),
  closeAfterNullYielded(),
]).then(mustCall());
