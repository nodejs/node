// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  ReadableStream,
  WritableStream,
} = require('node:stream/web');
const { internalBinding } = require('internal/test/binding');
const {
  isNonThenable,
  cloneAsUint8Array,
} = internalBinding('webstreams');

// The native helpers must be the ones the JS implementation actually calls.
assert.strictEqual(typeof isNonThenable, 'function');
assert.strictEqual(typeof cloneAsUint8Array, 'function');

assert.strictEqual(isNonThenable(undefined), true);
assert.strictEqual(isNonThenable(null), true);
assert.strictEqual(isNonThenable(1), true);
assert.strictEqual(isNonThenable('x'), true);
assert.strictEqual(isNonThenable(true), true);
assert.strictEqual(isNonThenable({}), false);
assert.strictEqual(isNonThenable(() => {}), false);
assert.strictEqual(isNonThenable(Promise.resolve()), false);
assert.strictEqual(isNonThenable(new Proxy({}, {})), false);
assert.strictEqual(isNonThenable(new Proxy(Object(1), {})), false);
assert.strictEqual(isNonThenable(new Proxy(() => {}, {})), false);

{
  const src = new Uint8Array([1, 2, 3, 4]);
  const cloned = cloneAsUint8Array(src);
  assert.ok(cloned instanceof Uint8Array);
  assert.deepStrictEqual([...cloned], [1, 2, 3, 4]);
  src[0] = 9;
  assert.strictEqual(cloned[0], 1);
}

{
  assert.throws(() => cloneAsUint8Array(1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Public API: pull-driven ReadableStream + read().
(async () => {
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('a');
      controller.enqueue('b');
      controller.close();
    },
  });
  const reader = rs.getReader();
  {
    const { value, done } = await reader.read();
    assert.strictEqual(value, 'a');
    assert.strictEqual(done, false);
  }
  {
    const { value, done } = await reader.read();
    assert.strictEqual(value, 'b');
    assert.strictEqual(done, false);
  }
  {
    const { value, done } = await reader.read();
    assert.strictEqual(value, undefined);
    assert.strictEqual(done, true);
  }
})().then(common.mustCall());

// Public API: pipeTo with a sync sink — the optimized write drain path.
(async () => {
  const expected = [];
  const received = [];
  const rs = new ReadableStream({
    start(controller) {
      for (let i = 0; i < 32; i++) {
        expected.push(i);
        controller.enqueue(i);
      }
      controller.close();
    },
  });
  await rs.pipeTo(new WritableStream({
    write(chunk) {
      received.push(chunk);
    },
  }));
  assert.deepStrictEqual(received, expected);
})().then(common.mustCall());

// Spec path: each pull is separated by a microtask. Start schedules one
// pull; further pulls wait for that fulfillment and do not run in the
// same turn.
{
  let calls = 0;
  new ReadableStream({
    pull(controller) {
      controller.enqueue(++calls);
    },
  }, {
    highWaterMark: 4,
  });
  queueMicrotask(common.mustCall(() => {
    assert.strictEqual(calls, 1);
    // The next pull is queued only after fulfillment, so it is not
    // invoked in this same turn.
    queueMicrotask(common.mustCall(() => {
      assert.strictEqual(calls, 2);
    }));
  }));
}

// pipeTo of a pull-driven source must deliver every chunk.
(async () => {
  const n = 64;
  let i = 0;
  const received = [];
  const rs = new ReadableStream({
    pull(controller) {
      if (i < n)
        controller.enqueue(i++);
      else
        controller.close();
    },
  }, { highWaterMark: 8 });
  await rs.pipeTo(new WritableStream({
    write(chunk) {
      received.push(chunk);
    },
  }, { highWaterMark: 8 }));
  assert.strictEqual(received.length, n);
  assert.deepStrictEqual(received, Array.from({ length: n }, (_, k) => k));
})().then(common.mustCall());

{
  // Empty-argument construction defers the controller. cancel() and
  // getReader() must still work on the public API.
  const rs = new ReadableStream();
  rs.cancel().then(common.mustCall());
}

{
  // Subclass that calls getReader() before the subclass constructor
  // finishes: the deferred controller is materialized then.
  class Sub extends ReadableStream {
    constructor() {
      super();
      this.reader = this.getReader();
    }
  }
  const rs = new Sub();
  assert.ok(rs.locked);
  rs.reader.cancel().then(common.mustCall());
}

{
  // Subclass that calls cancel() before the subclass constructor finishes.
  class Sub extends ReadableStream {
    constructor() {
      super();
      this.closed = this.cancel();
    }
  }
  const rs = new Sub();
  rs.closed.then(common.mustCall());
}

{
  // Passing a source leaves the empty-argument path, so start() receives
  // a controller during super() even if the subclass constructor later
  // calls getReader().
  let sawController = false;
  class Sub extends ReadableStream {
    constructor() {
      super({
        start(controller) {
          sawController = controller != null;
        },
      });
      this.reader = this.getReader();
    }
  }
  const rs = new Sub();
  assert.ok(rs.locked);
  queueMicrotask(common.mustCall(() => {
    assert.strictEqual(sawController, true);
    rs.reader.cancel().then(common.mustCall());
  }));
}

{
  const rs = new ReadableStream();
  const reader = rs.getReader();
  reader.cancel().then(common.mustCall());
}

{
  // A Proxy around a thenable must not take the non-thenable shortcut.
  let pulled = false;
  const thenable = new Proxy({
    then(resolve) {
      resolve();
    },
  }, {});
  const rs = new ReadableStream({
    pull(controller) {
      if (pulled) {
        controller.close();
        return thenable;
      }
      pulled = true;
      controller.enqueue('proxied');
      return thenable;
    },
  });
  rs.getReader().read().then(common.mustCall(({ value, done }) => {
    assert.strictEqual(value, 'proxied');
    assert.strictEqual(done, false);
  }));
}

{
  // Do not read controller.signal before abort(): the lazy AbortController
  // must still report the abort reason on first access.
  let ctrl;
  const err = new Error('hotpath-abort-before-signal');
  const ws = new WritableStream({
    start(c) { ctrl = c; },
  });
  ws.abort(err);
  assert.strictEqual(ctrl.signal.aborted, true);
  assert.strictEqual(ctrl.signal.reason, err);
}
