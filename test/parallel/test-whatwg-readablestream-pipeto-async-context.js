'use strict';

// The pipeTo() pump must keep running in the async context of the
// pipeTo() call when it resumes after backpressure or after a chunk
// arrives, instead of taking the context of the code that cleared the
// backpressure or enqueued the chunk. The source pulls and the writes the
// pump issues are checked.

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { setImmediate: tick } = require('timers/promises');

const als = new AsyncLocalStorage();

function store() {
  return als.getStore() ?? 'none';
}

// Settles the pending callbacks from the given context until `promise`
// settles.
async function settleUntil(promise, ctx, pending) {
  let finished = false;
  promise.then(common.mustCall(() => { finished = true; }));
  while (!finished) {
    await tick();
    while (pending.length > 0) als.run(ctx, pending.shift());
  }
  await promise;
}

const tests = [];
function test(name, expected, fn) {
  tests.push({ name, expected, fn });
}

test('sink writes resolved from another context', [
  // The first pull and write follow the source's start step.
  'pull:creator', 'write-1:creator',
  'pull:piper', 'write-2:piper',
  'pull:piper', 'write-3:piper',
  'pull:piper',
], async (seen) => {
  let n = 0;
  const pending = [];
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 3) c.close(); else c.enqueue(n);
    },
  }, { highWaterMark: 0 }));
  const ws = als.run('creator', () => new WritableStream({
    write(chunk) {
      seen.push(`write-${chunk}:${store()}`);
      const { promise, resolve } = Promise.withResolvers();
      pending.push(resolve);
      return promise;
    },
  }, { highWaterMark: 1 }));
  await settleUntil(als.run('piper', () => rs.pipeTo(ws)), 'releaser', pending);
});

test('push source under backpressure', [
  'write-a:pusher-a', 'write-b:piper', 'write-c:piper', 'write-d:piper',
], async (seen) => {
  let controller;
  const pending = [];
  const rs = als.run('creator', () => new ReadableStream({
    start(c) { controller = c; },
  }, { highWaterMark: 0 }));
  const ws = als.run('creator', () => new WritableStream({
    write(chunk) {
      seen.push(`write-${chunk}:${store()}`);
      const { promise, resolve } = Promise.withResolvers();
      pending.push(resolve);
      return promise;
    },
  }, { highWaterMark: 1 }));
  const piped = als.run('piper', () => rs.pipeTo(ws));
  await tick();
  for (const chunk of 'abcd')
    als.run(`pusher-${chunk}`, () => controller.enqueue(chunk));
  als.run('closer', () => controller.close());
  await settleUntil(piped, 'releaser', pending);
});

test('pipeThrough() with pulls resolved from another context', [
  'pull:piper', 'transform-1:resolver',
  'pull:piper', 'transform-2:resolver',
  'pull:piper', 'transform-3:resolver',
  'pull:piper',
], async (seen) => {
  let n = 0;
  const pending = [];
  const readable = als.run('piper', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      const { promise, resolve } = Promise.withResolvers();
      pending.push(() => {
        if (++n > 3) c.close(); else c.enqueue(n);
        resolve();
      });
      return promise;
    },
  }, { highWaterMark: 0 }).pipeThrough(new TransformStream({
    transform(chunk, c) {
      seen.push(`transform-${chunk}:${store()}`);
      c.enqueue(chunk);
    },
  })));
  const consumed = als.run('reader', async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable);
  });
  await settleUntil(consumed, 'resolver', pending);
});

test('pipeThrough() chain into a slow sink', [
  'pull:creator', 't1-1:piper', 't2-1:piper', 'write-1:piper',
  'pull:piper', 't1-2:piper', 't2-2:piper', 'write-2:piper',
  'pull:piper', 't1-3:piper', 't2-3:piper', 'write-3:piper',
  'pull:piper',
], async (seen) => {
  let n = 0;
  function transform(name) {
    return new TransformStream({
      transform(chunk, c) {
        seen.push(`${name}-${chunk}:${store()}`);
        c.enqueue(chunk);
      },
    });
  }
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 3) c.close(); else c.enqueue(n);
    },
  }, { highWaterMark: 0 }));
  const ws = als.run('creator', () => new WritableStream({
    async write(chunk) {
      seen.push(`write-${chunk}:${store()}`);
      await tick();
    },
  }, { highWaterMark: 1 }));
  await als.run('piper', () => rs.pipeThrough(transform('t1'))
    .pipeThrough(transform('t2')).pipeTo(ws));
});

(async () => {
  for (const { name, expected, fn } of tests) {
    const seen = [];
    await fn(seen);
    // Only the context of each callback is checked, not how the callbacks
    // of different streams interleave.
    assert.deepStrictEqual(seen.sort(), expected.toSorted(), name);
  }
})().then(common.mustCall());
