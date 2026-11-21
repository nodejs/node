'use strict';

require('../common');

const assert = require('node:assert');

const {
  EventEmitter
} = require('node:events');

const {
  Readable
} = require('node:stream');

const {
  concat,
  text
} = require('node:stream/consumers');

const {
  test,
} = require('node:test');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));


const collect = async (it) => {
  const out = [];
  for await (const chunk of it) out.push(String(chunk));
  return out;
};

// A stream that yields its chunks slowly, so siblings have time to misbehave.
const slow = (chunks, delay = 10) =>
  Readable.from((async function* () {
    for (const c of chunks) {
      await sleep(delay);
      yield c;
    }
  })());

// A live, idle stream that blows up on a timer with nobody reading it.
const bombs = (ms, err = new Error('boom')) => {
  const s = new Readable({ read() {} });
  setTimeout(() => s.destroy(err), ms);
  return s;
};

// ---------------------------------------------------------------------------
// signature
// ---------------------------------------------------------------------------


test('rejects a chunk as the collection', () => {
  assert.throws(() => concat('abc'), /must be an instance of Iterable or AsyncIterable/);
  assert.throws(() => concat(Buffer.from('abc')), /must be an instance of Iterable or AsyncIterable/);
});

test('rejects an invalid source inside the collection', async () => {
  await assert.rejects(
    collect(concat([Readable.from(['a']), 42])),
    /argument must be an instance of Stream, Iterable, or AsyncIterable/);
});

test('rejects a non-iterable collection', () => {
  assert.throws(() => concat(42), /must be an instance of Iterable or AsyncIterable/);
  assert.throws(() => concat(null), /must be an instance of Iterable or AsyncIterable/);
});

test('rejects a non-object options bag', () => {
  assert.throws(() => concat([], 'nope'), /argument must be of type object/);
  assert.throws(() => concat([], { signal: 'nope' }), /must be an instance of AbortSignal/);
});

// ---------------------------------------------------------------------------
// invariants
//
// These assert the guarding contract directly and use no timers. When the
// timing-dependent tests below them fail, these say whether the listeners were
// ever attached at all - the difference between a broken guard and a broken
// assertion.
// ---------------------------------------------------------------------------

test('every source is guarded before the first read', async () => {
  const sources = [
    new Readable({ read() {} }),
    new Readable({ read() {} }),
    new Readable({ read() {} }),
  ];

  const it = concat(sources);
  for (let i = 0; i < sources.length; i++) {
    assert.ok(sources[i].listenerCount('error') > 0,
              `sources[${i}] left unguarded at call time`);
  }
  await it.return();
});

test('a source read to completion keeps none of our listeners', async () => {
  // Node's own async iterator installs a permanent 'error' handler on any
  // stream it consumes, so the count never reaches zero. Measure against a
  // stream consumed without concat() to isolate the listeners we added.
  const control = Readable.from(['x']);
  for await (const chunk of control) assert.strictEqual(String(chunk), 'x');
  const baseline = control.listenerCount('error');

  const a = Readable.from(['a']);
  const b = Readable.from(['b']);
  assert.deepStrictEqual(await collect(concat([a, b])), ['a', 'b']);

  assert.strictEqual(a.listenerCount('error'), baseline); // Listeners left on a
  assert.strictEqual(b.listenerCount('error'), baseline); // Listeners left on b
});

test('a guarded source survives an error with nobody reading it', async () => {
  const idle = new Readable({ read() {} });
  const it = concat([new Readable({ read() {} }), idle]);

  // Synchronous destroy: an unguarded source throws here rather than as an
  // uncaught exception on a later tick, which is far easier to attribute.
  idle.destroy(new Error('guarded'));
  await assert.rejects(collect(it), /guarded/);
});

test('a source that failed before concat() saw it still reports', async () => {
  const dead = new Readable({ read() {} });
  dead.on('error', () => {});
  dead.destroy(new Error('already gone'));
  await sleep(5); // Let the 'error' event fire before concat() ever sees it

  await assert.rejects(
    collect(concat([Readable.from(['a']), dead])), /already gone/);
});

// ---------------------------------------------------------------------------
// basics
// ---------------------------------------------------------------------------

test('concatenates in order', async () => {
  const out = await collect(concat([
    Readable.from(['a', 'b']),
    Readable.from(['c']),
    Readable.from(['d', 'e']),
  ]));
  assert.deepStrictEqual(out, ['a', 'b', 'c', 'd', 'e']);
});

test('mixes streams, iterables and chunks', async () => {
  const out = await collect(concat([
    Readable.from(['a']),
    ['b', 'c'],
    'd',
    (function* () { yield 'e'; })(),
  ]));
  assert.deepStrictEqual(out, ['a', 'b', 'c', 'd', 'e']);
});

// The case mcollina raised on the PR: an unread sibling errors while we are
// still draining an earlier stream. Naively this is an unhandled 'error' event
// and the process dies. Here it must surface as a normal rejection.
test('an idle sibling erroring does not crash the process', async () => {
  const it = concat([slow(['a', 'b', 'c', 'd'], 20), bombs(15), Readable.from(['z'])]);
  await assert.rejects(collect(it), /boom/);
});

test('the failure aborts promptly, without draining the current stream', async () => {
  const started = Date.now();
  // Ten chunks at 20ms would take 200ms; the bomb goes off at 15ms.
  const it = concat([slow('0123456789'.split(''), 20), bombs(15)]);

  await assert.rejects(collect(it), /boom/);
  assert.ok(Date.now() - started < 120, `aborted late: ${Date.now() - started}ms`);
});

test('a failure destroys every other source', async () => {
  const a = slow(['a', 'b', 'c'], 20);
  const c = new Readable({ read() {} });

  await assert.rejects(collect(concat([a, bombs(15), c])), /boom/);
  await sleep(10);
  assert.ok(a.destroyed, 'current source destroyed');
  assert.ok(c.destroyed, 'queued source destroyed');
});

test('chunks yielded before a failure are not withdrawn', async () => {
  // Fail an unread sibling at a known point in the output rather than on a
  // timer, so the prefix is exact rather than a race between two delays.
  const bomb = new Readable({ read() {} });
  const it = concat([
    Readable.from(['a', 'b']),
    slow(['c', 'd'], 100), // Reached, but never gets to deliver
    Readable.from(['e']),  // Never reached
    bomb,
  ]);

  const captured = [];
  let error = null;

  try {
    for await (const chunk of it) {
      captured.push(String(chunk));
      if (captured.length === 2) bomb.destroy(new Error('boom'));
    }
  } catch (err) {
    error = err;
  }

  assert.ok(error, 'iteration should have failed');
  assert.strictEqual(error.message, 'boom');
  // Everything delivered before the failure is kept, in order, and nothing
  // from a later source leaks out after it.
  assert.deepStrictEqual(captured, ['a', 'b']);
});

test('a failure mid-source keeps the chunks already taken from it', async () => {
  const bomb = new Readable({ read() {} });
  const it = concat([
    Readable.from(['a']),
    slow(['b', 'c', 'd'], 100),
    bomb,
  ]);

  const captured = [];
  let error = null;

  try {
    for await (const chunk of it) {
      captured.push(String(chunk));
      // Fires partway through the second source, not between sources.
      if (captured.length === 2) bomb.destroy(new Error('boom'));
    }
  } catch (err) {
    error = err;
  }

  assert.ok(error, 'iteration should have failed');
  assert.strictEqual(error.message, 'boom');
  assert.deepStrictEqual(captured, ['a', 'b']);
});

test('a source is never pulled ahead of what was delivered', async () => {
  // Recovery depends on this. A caller that wraps a source to record its
  // progress must never record a chunk the consumer did not receive, or it
  // will resume from the wrong offset. concat() pulls one chunk at a time and
  // only on demand, so the wrapper cannot run ahead of the consumer.
  const state = { delivered: 0 };
  const bomb = new Readable({ read() {} });

  async function* tracked() {
    for (const c of ['aa', 'bb', 'cc', 'dd']) {
      await sleep(20);
      yield c;
      state.delivered += c.length; // Reached only once the consumer took it
    }
  };

  setTimeout(() => bomb.destroy(new Error('boom')), 50);

  const received = [];
  await assert.rejects(async () => {
    for await (const chunk of concat([tracked(), bomb])) {
      received.push(String(chunk));
    }
  }, /boom/);

  assert.strictEqual(state.delivered, received.join('').length);
});

test('the first error wins', async () => {
  const it = concat([
    slow(['a', 'b', 'c'], 30),
    bombs(10, new Error('first')),
    bombs(20, new Error('second')),
  ]);
  await assert.rejects(collect(it), /first/);
});

test('breaking early destroys unconsumed sources', async () => {
  const a = Readable.from(['a', 'b']);
  const b = new Readable({ read() {} });
  const c = new Readable({ read() {} });

  for await (const chunk of concat([a, b, c])) {
    assert.strictEqual(String(chunk), 'a');
    break;
  }
  await sleep(10);
  assert.ok(a.destroyed && b.destroyed && c.destroyed);
});

test('an error inside the current source propagates', async () => {
  const bad = Readable.from((async function* () {
    yield 'a';
    throw new Error('inner');
  })());
  const next = new Readable({ read() {} });

  await assert.rejects(collect(concat([bad, next])), /inner/);
  await sleep(10);
  assert.ok(next.destroyed, 'queued source destroyed');
});

// ---------------------------------------------------------------------------
// options — the slot the variadic signature would have cost us
// ---------------------------------------------------------------------------

test('signal: aborting mid-read fails the iterator', async () => {
  const ac = new AbortController();
  const b = new Readable({ read() {} });
  setTimeout(() => ac.abort(), 15);

  await assert.rejects(
    collect(concat([slow(['a', 'b', 'c'], 25), b], { signal: ac.signal })),
    (err) => err.name === 'AbortError');
  await sleep(10);
  assert.ok(b.destroyed, 'queued source destroyed on abort');
});

test('aborting is the only way out of a stalled source', async () => {
  // A source that never produces and never ends cannot be escaped with break
  // or return(): an async generator suspended at an await cannot be
  // force-returned, the request just queues behind the pending read. The
  // signal is the escape hatch - and teardown must destroy before releasing
  // the iterators, or it deadlocks on the read it abandoned.
  const ac = new AbortController();
  const stalled = new Readable({ read() {} });
  const queued = new Readable({ read() {} });
  setTimeout(() => ac.abort(), 15);

  await assert.rejects(
    collect(concat([stalled, queued], { signal: ac.signal })),
    (err) => err.name === 'AbortError');

  await sleep(10);
  assert.ok(stalled.destroyed, 'stalled source destroyed');
  assert.ok(queued.destroyed, 'queued source destroyed');
});

test('signal: an already-aborted signal fails on first pull', async () => {
  const signal = AbortSignal.abort(new Error('too late'));
  await assert.rejects(collect(concat([Readable.from(['a'])], { signal })),
                       /too late/);
});

test('signal: a clean run does not leave a listener behind', async () => {
  const ac = new AbortController();
  await collect(concat([Readable.from(['a'])], { signal: ac.signal }));
  // Node exposes listener counts on AbortSignal via the events module.
  const { listenerCount } = await import('node:events');
  assert.strictEqual(listenerCount(ac.signal, 'abort'), 0);
});

test('lazy: true forces one-at-a-time over a sync generator', async () => {
  let created = 0;
  function* producer() {
    for (const s of ['a', 'b', 'c']) {
      created++;
      yield Readable.from([s]);
    }
  }

  for await (const chunk of concat(producer(), { lazy: true })) {
    assert.strictEqual(String(chunk), 'a');
    break;
  }
  assert.strictEqual(created, 1, `created ${created} sources, expected 1`);
});

test('lazy: default drains a sync generator, and guards what it finds', async () => {
  let created = 0;
  function* producer() {
    created++;
    yield slow(['a', 'b', 'c'], 20);
    created++;
    yield bombs(15);
  }

  const it = concat(producer());
  assert.strictEqual(created, 2); // Drained at call time
  await assert.rejects(collect(it), /boom/);
});

test('lazy: false drains a bounded async producer', async () => {
  async function* producer() {
    yield Readable.from(['a']);
    await sleep(5);
    yield Readable.from(['b']);
  }
  assert.deepStrictEqual(await collect(concat(producer(), { lazy: false })), ['a', 'b']);
});

test('destroyOnReturn: false hands sources back alive', async () => {
  const b = new Readable({ read() {} });

  for await (const chunk of concat([Readable.from(['a']), b], { destroyOnReturn: false })) {
    assert.strictEqual(String(chunk), 'a');
    break;
  }
  await sleep(10);
  assert.ok(!b.destroyed, 'source left alive for the caller');
  assert.strictEqual(b.listenerCount('error'), 0); // And left unguarded
  b.destroy();
});

// ---------------------------------------------------------------------------
// laziness
// ---------------------------------------------------------------------------

test('an async producer is pulled one source at a time', async () => {
  let created = 0;
  async function* producer() {
    for (const s of ['a', 'b', 'c']) {
      created++;
      yield Readable.from([s]);
    }
  }

  for await (const chunk of concat(producer())) {
    assert.strictEqual(String(chunk), 'a');
    break;
  }
  assert.strictEqual(created, 1, `created ${created} sources, expected 1`);
});

test('factories in a sync collection stay inert until reached', async () => {
  let opened = 0;
  const open = (s) => () => { opened++; return Readable.from([s]); };

  const it = concat([open('a'), open('b'), open('c')]);
  assert.strictEqual(opened, 0); // Nothing opened at call time

  for await (const chunk of it) {
    assert.strictEqual(String(chunk), 'a');
    break;
  }
  assert.strictEqual(opened, 1, `opened ${opened}, expected 1`);
});

test('factories are still guarded once materialized', async () => {
  await assert.rejects(
    collect(concat([() => slow(['a', 'b', 'c'], 20), () => bombs(5)])), /boom/);
});

test('async factories are awaited', async () => {
  const out = await collect(concat([
    async () => Readable.from(['a']),
    async () => { await sleep(5); return Readable.from(['b']); },
  ]));
  assert.deepStrictEqual(out, ['a', 'b']);
});

// ---------------------------------------------------------------------------
// source variety (ronag's "this doesn't support old streams", jasnell's
// "make it work with any stream.Readable, ReadableStream, and async iterable")
// ---------------------------------------------------------------------------

test('supports legacy streams1 sources', async () => {
  const legacy = new EventEmitter();
  legacy.readable = true;
  legacy.pause = () => {};
  legacy.resume = () => {};
  legacy.pipe = () => {};
  setTimeout(() => {
    legacy.emit('data', 'old');
    legacy.emit('end');
  }, 5);

  const out = await collect(concat([legacy, Readable.from(['new'])]));
  assert.deepStrictEqual(out, ['old', 'new']);
});

test('supports web ReadableStream sources', async () => {
  const web = new ReadableStream({
    start(c) { c.enqueue('w1'); c.enqueue('w2'); c.close(); },
  });
  const out = await collect(concat([web, Readable.from(['n'])]));
  assert.deepStrictEqual(out, ['w1', 'w2', 'n']);
});

test('a failing web stream surfaces as a rejection', async () => {
  const web = new ReadableStream({
    start(c) { c.enqueue('w1'); c.error(new Error('web boom')); },
  });
  await assert.rejects(collect(concat([web])), /web boom/);
});

// ---------------------------------------------------------------------------
// composition — the point of returning an iterator rather than a stream
// ---------------------------------------------------------------------------

test('Readable.from(concat(...)) round-trips', async () => {
  const r = Readable.from(concat([Readable.from(['a']), Readable.from(['b'])]));
  assert.ok(r instanceof Readable);
  assert.deepStrictEqual(await collect(r), ['a', 'b']);
});

test('consumers.text(concat(...)) works', async () => {
  const value = await text(Readable.from(concat([
    Readable.from(['hello, ']),
    Readable.from(['world']),
  ])));
  assert.strictEqual(value, 'hello, world');
});

test('objects pass through untouched — no objectMode to guess', async () => {
  const out = [];
  for await (const obj of concat([
    Readable.from([{ id: 1 }]),
    Readable.from([{ id: 2 }]),
  ])) out.push(obj);

  assert.deepStrictEqual(out, [{ id: 1 }, { id: 2 }]);
});
