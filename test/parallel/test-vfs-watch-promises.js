// Flags: --experimental-vfs
'use strict';

// promises.watch() returns an async iterable. Cover its event queue,
// next/return/throw, and close-while-pending behaviour.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

(async () => {
  // Basic for-await iteration receives a change event
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/file.txt', 'a');
    const iter = myVfs.promises.watch('/file.txt', { interval: 25 });
    queueMicrotask(() => myVfs.writeFileSync('/file.txt', 'longer-changed'));
    for await (const evt of iter) {
      assert.strictEqual(evt.eventType, 'change');
      break;
    }
  }

  // Events queued before next() drain via the next call
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q.txt', 'a');
    const iter = myVfs.promises.watch('/q.txt', { interval: 25 });
    myVfs.writeFileSync('/q.txt', 'longer-changed');
    assert.partialDeepStrictEqual(await iter.next(), {
      done: false,
      value: {
        eventType: 'change',
      }
    });
    assert.deepStrictEqual(await iter.return(), {
      done: true,
      value: undefined,
    });
  }

  // A change while a next() is pending shifts the resolver
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q2.txt', 'a');
    const iter = myVfs.promises.watch('/q2.txt', { interval: 25 });
    const pending = iter.next();
    myVfs.writeFileSync('/q2.txt', 'longer-changed');
    const r = await pending;
    if (!r.done) assert.strictEqual(r.value.eventType, 'change');
    assert.partialDeepStrictEqual(await pending, {
      done: false,
      value: {
        eventType: 'change',
      }
    });
    assert.deepStrictEqual(await iter.return(), {
      done: true,
      value: undefined,
    });
  }

  // throw() closes the watcher and resolves with done:true
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q3.txt', 'a');
    const iter = myVfs.promises.watch('/q3.txt', { interval: 25 });
    const r = await iter.throw(new Error('go away'));
    assert.deepStrictEqual(r, { done: true, value: undefined });
    myVfs.writeFileSync('/q3.txt', 'b');
    assert.deepStrictEqual(await iter.next(), { done: true, value: undefined });
  }

  // Close while a resolver is pending — drains via the 'close' handler
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q4.txt', 'a');
    const iter = myVfs.promises.watch('/q4.txt', { interval: 1000 });
    const pending = iter.next();
    queueMicrotask(() => iter.return());
    const r = await pending;
    assert.strictEqual(r.done, true);
  }
})().then(common.mustCall());
