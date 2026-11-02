'use strict';

// Writes through Writable.fromWeb() and Duplex.fromWeb() must reach the
// WritableStream sink in the async context of the Node.js write, also when
// they are issued before the WritableStream has started.

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { Duplex, Writable } = require('stream');
const { setImmediate: tick } = require('timers/promises');

const als = new AsyncLocalStorage();

function store() {
  return als.getStore() ?? 'none';
}

function createSink(seen, start) {
  return new WritableStream({
    start,
    write(chunk) { seen.push(`write-${chunk}:${store()}`); },
    close() { seen.push(`close:${store()}`); },
  });
}

const tests = [];
function test(name, expected, fn) {
  tests.push({ name, expected, fn });
}

test('Writable.fromWeb() written before start', [
  'write-a:writer', 'write-b:writer', 'close:writer',
], async (seen) => {
  const ws = als.run('creator', () => createSink(seen));
  const writable = Writable.fromWeb(ws, { decodeStrings: false });
  await als.run('writer', () => new Promise((resolve) => {
    writable.write('a');
    writable.write('b');
    writable.end(resolve);
  }));
});

test('Writable.fromWeb() writev before start', [
  'write-a:writer', 'write-b:writer', 'close:writer',
], async (seen) => {
  const ws = als.run('creator', () => createSink(seen));
  const writable = Writable.fromWeb(ws, { decodeStrings: false });
  await als.run('writer', () => new Promise((resolve) => {
    writable.cork();
    writable.write('a');
    writable.write('b');
    writable.uncork();
    writable.end(resolve);
  }));
});

test('Writable.fromWeb() written during an async start', [
  // The WritableStream processes the chunk queued while it was starting
  // from its start step. The Node.js write callback still runs in the
  // writer's context, so the writes it releases do too.
  'write-a:creator', 'write-b:writer', 'close:writer',
], async (seen) => {
  const ws = als.run('creator', () => createSink(seen, () => tick()));
  const writable = Writable.fromWeb(ws, { decodeStrings: false });
  await als.run('writer', () => new Promise((resolve) => {
    writable.write('a');
    writable.write('b');
    writable.end(resolve);
  }));
});

test('Writable.fromWeb() written after start', [
  'write-a:w1', 'write-b:w2', 'close:ender',
], async (seen) => {
  const ws = als.run('creator', () => createSink(seen));
  const writable = Writable.fromWeb(ws, { decodeStrings: false });
  await tick();
  await als.run('w1', () => new Promise((resolve) => writable.write('a', resolve)));
  await als.run('w2', () => new Promise((resolve) => writable.write('b', resolve)));
  await als.run('ender', () => new Promise((resolve) => writable.end(resolve)));
});

test('Duplex.fromWeb() written before start', [
  'write-a:writer', 'close:writer',
], async (seen) => {
  const pair = als.run('creator', () => ({
    readable: new ReadableStream({ start(c) { c.close(); } }),
    writable: createSink(seen),
  }));
  const duplex = Duplex.fromWeb(pair, { decodeStrings: false });
  duplex.resume();
  await als.run('writer', () => new Promise((resolve) => {
    duplex.write('a');
    duplex.end(resolve);
  }));
});

(async () => {
  for (const { name, expected, fn } of tests) {
    const seen = [];
    await fn(seen);
    assert.deepStrictEqual(seen, expected, name);
  }
})().then(common.mustCall());
