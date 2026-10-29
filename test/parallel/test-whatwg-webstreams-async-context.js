'use strict';

// Underlying source, sink and transformer callbacks must observe the async
// context the spec's promise reactions would carry. Internal scheduling
// shortcuts (parked continuations, shared resolved promises, skipped start
// steps) must preserve it.
// Refs: https://github.com/nodejs/node/issues/66906

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { setImmediate: tick } = require('timers/promises');

const als = new AsyncLocalStorage();

function store() {
  return als.getStore() ?? 'none';
}

async function consume(readable) {
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable);
}

const tests = [];
function test(name, expected, fn) {
  tests.push({ name, expected, fn });
}

test('ReadableStream default reader', [
  'start:creator', 'pull:reader', 'after-read:reader', 'pull:reader',
  'cancel:canceler',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    start() { seen.push(`start:${store()}`); },
    pull(c) {
      seen.push(`pull:${store()}`);
      c.enqueue(++n);
    },
    cancel() { seen.push(`cancel:${store()}`); },
  }, { highWaterMark: 0 }));
  await tick();
  const r = rs.getReader();
  await als.run('reader', async () => {
    await r.read();
    seen.push(`after-read:${store()}`);
    await r.read();
  });
  r.releaseLock();
  await als.run('canceler', () => rs.cancel());
});

test('ReadableStream pull to fill the queue', [
  'pull:creator', 'pull:creator', 'pull:reader', 'chunk:reader',
  'pull:reader', 'chunk:reader', 'chunk:reader',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 3) c.close(); else c.enqueue(n);
    },
  }, { highWaterMark: 2 }));
  await tick();
  await als.run('reader', async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of rs) seen.push(`chunk:${store()}`);
  });
});

test('ReadableStream async pull', [
  'pull:creator', 'pull-resumed:creator',
  'pull:creator', 'pull-resumed:creator',
  'pull:creator', 'pull-resumed:creator',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    async pull(c) {
      seen.push(`pull:${store()}`);
      await tick();
      seen.push(`pull-resumed:${store()}`);
      if (++n > 2) c.close(); else c.enqueue(n);
    },
  }));
  await tick();
  await als.run('reader', () => consume(rs));
});

test('ReadableStream cancel from async iterator return()', [
  'pull:creator', 'pull:reader', 'chunk:reader', 'cancel:reader',
], async (seen) => {
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      c.enqueue(1);
    },
    cancel() { seen.push(`cancel:${store()}`); },
  }));
  await tick();
  await als.run('reader', async () => {
    const iterator = rs.values();
    await iterator.next();
    seen.push(`chunk:${store()}`);
    await iterator.return();
  });
});

test('ReadableByteStream with BYOB reader', [
  'start:creator',
  'pull:reader', 'read:reader',
  'pull:reader', 'read:reader',
  'pull:reader', 'read:reader',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    type: 'bytes',
    start() { seen.push(`start:${store()}`); },
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 2) {
        c.close();
        c.byobRequest.respond(0);
        return;
      }
      c.byobRequest.view[0] = n;
      c.byobRequest.respond(1);
    },
  }));
  await tick();
  await als.run('reader', async () => {
    const r = rs.getReader({ mode: 'byob' });
    let result;
    do {
      result = await r.read(new Uint8Array(4));
      seen.push(`read:${store()}`);
    } while (!result.done);
  });
});

test('ReadableByteStream with autoAllocateChunkSize', [
  'pull:reader', 'pull:reader', 'pull:reader',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    type: 'bytes',
    autoAllocateChunkSize: 4,
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 2) return c.close();
      c.enqueue(new Uint8Array([n]));
    },
  }));
  await tick();
  await als.run('reader', () => consume(rs));
});

test('ReadableStream.from() async iterable', [
  'gen-start:reader', 'gen-resumed:reader',
], async (seen) => {
  const rs = als.run('creator', () => ReadableStream.from((async function*() {
    seen.push(`gen-start:${store()}`);
    yield 1;
    await tick();
    seen.push(`gen-resumed:${store()}`);
    yield 2;
  })()));
  await tick();
  await als.run('reader', () => consume(rs));
});

test('ReadableStream.from() sync iterable', [
  'gen-start:reader', 'gen-resumed:reader',
], async (seen) => {
  const rs = als.run('creator', () => ReadableStream.from((function*() {
    seen.push(`gen-start:${store()}`);
    yield 1;
    seen.push(`gen-resumed:${store()}`);
    yield 2;
  })()));
  await tick();
  await als.run('reader', () => consume(rs));
});

test('ReadableStream tee()', [
  'pull:creator', 'pull:creator', 'pull:reader1',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 2) c.close(); else c.enqueue(n);
    },
  }));
  const [a, b] = als.run('teer', () => rs.tee());
  await tick();
  await als.run('reader1', () => consume(a));
  await als.run('reader2', () => consume(b));
});

test('ReadableByteStream tee()', [
  'pull:reader1', 'pull:reader1', 'pull:reader1',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    type: 'bytes',
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 2) {
        c.close();
        c.byobRequest?.respond(0);
        return;
      }
      c.enqueue(new Uint8Array([n]));
    },
  }));
  const [a, b] = als.run('teer', () => rs.tee());
  await tick();
  await als.run('reader1', async () => {
    const r = a.getReader({ mode: 'byob' });
    while (!(await r.read(new Uint8Array(4))).done);
  });
  await als.run('reader2', () => consume(b));
});

test('WritableStream sequential writes', [
  'start:creator', 'write-a:w1', 'write-b:w2', 'close:closer',
], async (seen) => {
  const ws = als.run('creator', () => new WritableStream({
    start() { seen.push(`start:${store()}`); },
    write(chunk) { seen.push(`write-${chunk}:${store()}`); },
    close() { seen.push(`close:${store()}`); },
  }));
  await tick();
  const w = ws.getWriter();
  await als.run('w1', () => w.write('a'));
  await als.run('w2', () => w.write('b'));
  await als.run('closer', () => w.close());
});

test('WritableStream queued writes', [
  // Queued chunks are processed from the reaction to the previous write,
  // which was registered in the context of the first write.
  'write-a:w1', 'write-a-resumed:w1',
  'write-b:w1', 'write-b-resumed:w1',
  'write-c:w1', 'write-c-resumed:w1',
  'close:w1',
], async (seen) => {
  const ws = als.run('creator', () => new WritableStream({
    async write(chunk) {
      seen.push(`write-${chunk}:${store()}`);
      await tick();
      seen.push(`write-${chunk}-resumed:${store()}`);
    },
    close() { seen.push(`close:${store()}`); },
  }, { highWaterMark: 10 }));
  await tick();
  const w = ws.getWriter();
  await Promise.all([
    als.run('w1', () => w.write('a')),
    als.run('w2', () => w.write('b')),
    als.run('w3', () => w.write('c')),
    als.run('closer', () => w.close()),
  ]);
});

test('WritableStream writes queued during an async start()', [
  'start-resumed:creator', 'write-a:creator', 'write-b:creator',
  'close:creator',
], async (seen) => {
  const ws = als.run('creator', () => new WritableStream({
    async start() {
      await tick();
      seen.push(`start-resumed:${store()}`);
    },
    write(chunk) { seen.push(`write-${chunk}:${store()}`); },
    close() { seen.push(`close:${store()}`); },
  }));
  const w = ws.getWriter();
  await Promise.all([
    als.run('w1', () => w.write('a')),
    als.run('w2', () => w.write('b')),
    als.run('closer', () => w.close()),
  ]);
});

test('WritableStream writer.ready after backpressure', [
  'ready:waiter',
], async (seen) => {
  let release;
  const ws = als.run('creator', () => new WritableStream({
    write() {
      return new Promise((resolve) => { release = resolve; });
    },
  }, { highWaterMark: 1 }));
  await tick();
  const w = ws.getWriter();
  als.run('w1', () => w.write('a'));
  als.run('w2', () => w.write('b'));
  const ready = als.run('waiter', () => w.ready.then(() => {
    seen.push(`ready:${store()}`);
  }));
  await tick();
  als.run('releaser', () => release());
  await tick();
  als.run('releaser', () => release());
  await ready;
});

test('WritableStream abort() during an in-flight write', [
  'write:w1', 'abort:w1',
], async (seen) => {
  let release;
  const ws = als.run('creator', () => new WritableStream({
    write() {
      seen.push(`write:${store()}`);
      return new Promise((resolve) => { release = resolve; });
    },
    abort() { seen.push(`abort:${store()}`); },
  }));
  await tick();
  const w = ws.getWriter();
  als.run('w1', () => w.write('a'));
  await tick();
  const aborted = als.run('aborter', () => w.abort(new Error('boom')));
  await tick();
  als.run('releaser', () => release());
  await aborted;
});

test('WritableStream abort() while idle', [
  'abort:aborter',
], async (seen) => {
  const ws = als.run('creator', () => new WritableStream({
    abort() { seen.push(`abort:${store()}`); },
  }));
  await tick();
  await als.run('aborter', () => ws.abort(new Error('boom')));
});

test('TransformStream writes before reads', [
  'start:creator', 'transform-a:w1', 'transform-b:w1', 'flush:w1',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream({
    start() { seen.push(`start:${store()}`); },
    transform(chunk, c) {
      seen.push(`transform-${chunk}:${store()}`);
      c.enqueue(chunk);
    },
    flush() { seen.push(`flush:${store()}`); },
  }));
  await tick();
  const w = ts.writable.getWriter();
  const r = ts.readable.getReader();
  const writes = [
    als.run('w1', () => w.write('a')),
    als.run('w2', () => w.write('b')),
    als.run('closer', () => w.close()),
  ];
  await tick();
  await als.run('reader', async () => {
    while (!(await r.read()).done);
  });
  await Promise.all(writes);
});

test('TransformStream reads before writes', [
  'transform-a:w1', 'transform-b:w2', 'flush:closer',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream({
    transform(chunk, c) {
      seen.push(`transform-${chunk}:${store()}`);
      c.enqueue(chunk);
    },
    flush() { seen.push(`flush:${store()}`); },
  }));
  await tick();
  const w = ts.writable.getWriter();
  const r = ts.readable.getReader();
  const reads = als.run('reader', async () => {
    while (!(await r.read()).done);
  });
  await tick();
  await als.run('w1', () => w.write('a'));
  await als.run('w2', () => w.write('b'));
  await als.run('closer', () => w.close());
  await reads;
});

test('TransformStream async transform() and flush()', [
  'transform-a:w1', 'transform-a-resumed:w1',
  'transform-b:w1', 'transform-b-resumed:w1',
  'flush-resumed:w1',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream({
    async transform(chunk, c) {
      seen.push(`transform-${chunk}:${store()}`);
      await tick();
      seen.push(`transform-${chunk}-resumed:${store()}`);
      c.enqueue(chunk);
    },
    async flush() {
      await tick();
      seen.push(`flush-resumed:${store()}`);
    },
  }, { highWaterMark: 4 }, { highWaterMark: 4 }));
  await tick();
  const w = ts.writable.getWriter();
  const writes = [
    als.run('w1', () => w.write('a')),
    als.run('w2', () => w.write('b')),
    als.run('closer', () => w.close()),
  ];
  await als.run('reader', () => consume(ts.readable));
  await Promise.all(writes);
});

test('TransformStream readable.cancel()', [
  'cancel:canceler',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream({
    cancel() { seen.push(`cancel:${store()}`); },
  }));
  await tick();
  await als.run('canceler', () => ts.readable.cancel(new Error('boom')));
});

test('TransformStream writable.abort()', [
  'cancel:aborter',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream({
    cancel() { seen.push(`cancel:${store()}`); },
  }));
  await tick();
  await als.run('aborter', () => ts.writable.abort(new Error('boom')));
});

test('identity TransformStream piped to a WritableStream', [
  'write-a:w1', 'write-b:w2', 'close:piper',
], async (seen) => {
  const ts = als.run('creator', () => new TransformStream());
  const ws = als.run('creator', () => new WritableStream({
    write(chunk) { seen.push(`write-${chunk}:${store()}`); },
    close() { seen.push(`close:${store()}`); },
  }));
  await tick();
  const w = ts.writable.getWriter();
  const piped = als.run('piper', () => ts.readable.pipeTo(ws));
  await als.run('w1', () => w.write('a'));
  await als.run('w2', () => w.write('b'));
  await als.run('closer', () => w.close());
  await piped;
});

test('pipeTo()', [
  'pull:creator',
  'pull:piper', 'write-1:piper',
  'pull:piper', 'write-2:piper',
  'pull:piper', 'write-3:piper',
  'close:piper',
], async (seen) => {
  let n = 0;
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 3) c.close(); else c.enqueue(n);
    },
  }));
  const ws = als.run('creator', () => new WritableStream({
    write(chunk) { seen.push(`write-${chunk}:${store()}`); },
    close() { seen.push(`close:${store()}`); },
  }));
  await tick();
  await als.run('piper', () => rs.pipeTo(ws));
});

test('pipeTo() with backpressure', [
  'write-a:piper', 'write-b:piper', 'write-c:piper', 'write-d:piper',
  'close:piper',
], async (seen) => {
  const rs = als.run('creator', () => new ReadableStream({
    start(c) {
      for (const chunk of 'abcd') c.enqueue(chunk);
      c.close();
    },
  }));
  const ws = als.run('creator', () => new WritableStream({
    async write(chunk) {
      seen.push(`write-${chunk}:${store()}`);
      await tick();
    },
    close() { seen.push(`close:${store()}`); },
  }, { highWaterMark: 1 }));
  await tick();
  await als.run('piper', () => rs.pipeTo(ws));
});

test('pipeTo() aborted by a signal', [
  'abort:aborter', 'cancel:aborter',
], async (seen) => {
  let release;
  const rs = als.run('creator', () => new ReadableStream({
    pull(c) { c.enqueue(1); },
    cancel() { seen.push(`cancel:${store()}`); },
  }));
  const ws = als.run('creator', () => new WritableStream({
    write() {
      return new Promise((resolve) => { release = resolve; });
    },
    abort() { seen.push(`abort:${store()}`); },
  }));
  const ac = new AbortController();
  const piped = als.run('piper', () => rs.pipeTo(ws, { signal: ac.signal }));
  await tick();
  als.run('aborter', () => ac.abort());
  await tick();
  als.run('releaser', () => release());
  await assert.rejects(piped, { name: 'AbortError' });
});

test('pipeTo() source errored', [
  'abort:piper',
], async (seen) => {
  let controller;
  const rs = als.run('creator', () => new ReadableStream({
    start(c) { controller = c; },
  }));
  const ws = als.run('creator', () => new WritableStream({
    abort() { seen.push(`abort:${store()}`); },
  }));
  const piped = als.run('piper', () => rs.pipeTo(ws));
  await tick();
  als.run('errorer', () => controller.error(new Error('boom')));
  await assert.rejects(piped, { message: 'boom' });
});

test('pipeTo() destination errored', [
  'cancel:piper',
], async (seen) => {
  let controller;
  const rs = als.run('creator', () => new ReadableStream({
    cancel() { seen.push(`cancel:${store()}`); },
  }));
  const ws = als.run('creator', () => new WritableStream({
    start(c) { controller = c; },
  }));
  const piped = als.run('piper', () => rs.pipeTo(ws));
  await tick();
  als.run('errorer', () => controller.error(new Error('boom')));
  await assert.rejects(piped, { message: 'boom' });
});

test('pipeThrough() with a parked write', [
  'transform-a:writer', 'transform-b:writer', 'transform-c:writer',
  'flush:writer',
], async (seen) => {
  const readable = als.run('writer', () => new ReadableStream({
    start(c) {
      for (const chunk of 'abc') c.enqueue(chunk);
      c.close();
    },
  }).pipeThrough(new TransformStream({
    transform(chunk, c) {
      seen.push(`transform-${chunk}:${store()}`);
      c.enqueue(chunk);
    },
    flush() { seen.push(`flush:${store()}`); },
  })));
  // Let the pipe park its first write on backpressure.
  await tick();
  await als.run('reader', () => consume(readable));
});

test('pipeThrough() chain', [
  'pull:piper', 'pull:piper', 'pull:piper',
  't1-1:piper', 't1-2:piper', 't1-flush:piper',
  't2-1:piper', 't2-2:piper', 't2-flush:piper',
], async (seen) => {
  let n = 0;
  function transform(name) {
    return new TransformStream({
      transform(chunk, c) {
        seen.push(`${name}-${chunk}:${store()}`);
        c.enqueue(chunk);
      },
      flush() { seen.push(`${name}-flush:${store()}`); },
    });
  }
  const readable = als.run('piper', () => new ReadableStream({
    pull(c) {
      seen.push(`pull:${store()}`);
      if (++n > 2) c.close(); else c.enqueue(n);
    },
  }, { highWaterMark: 0 }).pipeThrough(transform('t1'))
    .pipeThrough(transform('t2')));
  await tick();
  await als.run('reader', () => consume(readable));
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
