'use strict';

const { mustCall, mustNotCall } = require('../common');
const { Readable } = require('stream');
const { strictEqual } = require('assert');

async function asyncSupport() {
  const finallyMustCall = mustCall();
  async function* generate() {
    try {
      yield 'a';
      mustNotCall('only first item is read');
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  for await (const chunk of stream) {
    strictEqual(chunk, 'a');
    break;
  }
}

asyncSupport().then(mustCall());

async function syncSupport() {
  const finallyMustCall = mustCall();
  function* generate() {
    try {
      yield 'a';
      mustNotCall('only first item is read');
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  for await (const chunk of stream) {
    strictEqual(chunk, 'a');
    break;
  }
}

syncSupport().then(mustCall());

async function syncPromiseSupport() {
  const finallyMustCall = mustCall();
  function* generate() {
    try {
      yield Promise.resolve('a');
      mustNotCall('only first item is read');
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  for await (const chunk of stream) {
    strictEqual(chunk, 'a');
    break;
  }
}

syncPromiseSupport().then(mustCall());

async function syncRejectedSupport() {
  const finallyMustCall = mustCall();
  const noBodyCall = mustNotCall();
  const catchMustCall = mustCall();

  function* generate() {
    try {
      yield Promise.reject('a');
      mustNotCall();
    } finally {
      finallyMustCall();
    }
  }

  const stream = Readable.from(generate());

  try {
    for await (const chunk of stream) {
      noBodyCall(chunk);
    }
  } catch {
    catchMustCall();
  }
}

syncRejectedSupport().then(mustCall());

async function noReturnAfterThrow() {
  const returnMustNotCall = mustNotCall();
  const noBodyCall = mustNotCall();
  const catchMustCall = mustCall();

  const stream = Readable.from({
    [Symbol.asyncIterator]() { return this; },
    async next() { throw new Error('a'); },
    async return() { returnMustNotCall(); return { done: true }; },
  });

  try {
    for await (const chunk of stream) {
      noBodyCall(chunk);
    }
  } catch {
    catchMustCall();
  }
}

noReturnAfterThrow().then(mustCall());
