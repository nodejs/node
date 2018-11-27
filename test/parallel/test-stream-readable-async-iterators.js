'use strict';

const common = require('../common');
const { Readable, PassThrough, pipeline } = require('stream');
const assert = require('assert');

async function tests() {
  {
    const AsyncIteratorPrototype = Object.getPrototypeOf(
      Object.getPrototypeOf(async function* () {}).prototype);
    const rs = new Readable({});
    assert.strictEqual(
      Object.getPrototypeOf(Object.getPrototypeOf(rs[Symbol.asyncIterator]())),
      AsyncIteratorPrototype);
  }

  await (async function() {
    const readable = new Readable({ objectMode: true, read() {} });
    readable.push(0);
    readable.push(1);
    readable.push(null);

    const iter = readable[Symbol.asyncIterator]();
    assert.strictEqual((await iter.next()).value, 0);
    for await (const d of iter) {
      assert.strictEqual(d, 1);
    }
  })();

  await (async function() {
    console.log('read without for..await');
    const max = 5;
    const readable = new Readable({
      objectMode: true,
      read() {}
    });

    const iter = readable[Symbol.asyncIterator]();
    assert.strictEqual(iter.stream, readable);
    const values = [];
    for (let i = 0; i < max; i++) {
      values.push(iter.next());
    }
    Promise.all(values).then(common.mustCall((values) => {
      values.forEach(common.mustCall(
        (item, i) => assert.strictEqual(item.value, 'hello-' + i), 5));
    }));

    readable.push('hello-0');
    readable.push('hello-1');
    readable.push('hello-2');
    readable.push('hello-3');
    readable.push('hello-4');
    readable.push(null);

    const last = await iter.next();
    assert.strictEqual(last.done, true);
  })();

  await (async function() {
    console.log('read without for..await deferred');
    const readable = new Readable({
      objectMode: true,
      read() {}
    });

    const iter = readable[Symbol.asyncIterator]();
    assert.strictEqual(iter.stream, readable);
    let values = [];
    for (let i = 0; i < 3; i++) {
      values.push(iter.next());
    }

    readable.push('hello-0');
    readable.push('hello-1');
    readable.push('hello-2');

    let k = 0;
    const results1 = await Promise.all(values);
    results1.forEach(common.mustCall(
      (item) => assert.strictEqual(item.value, 'hello-' + k++), 3));

    values = [];
    for (let i = 0; i < 2; i++) {
      values.push(iter.next());
    }

    readable.push('hello-3');
    readable.push('hello-4');
    readable.push(null);

    const results2 = await Promise.all(values);
    results2.forEach(common.mustCall(
      (item) => assert.strictEqual(item.value, 'hello-' + k++), 2));

    const last = await iter.next();
    assert.strictEqual(last.done, true);
  })();

  await (async function() {
    console.log('read without for..await with errors');
    const max = 3;
    const readable = new Readable({
      objectMode: true,
      read() {}
    });

    const iter = readable[Symbol.asyncIterator]();
    assert.strictEqual(iter.stream, readable);
    const values = [];
    const errors = [];
    let i;
    for (i = 0; i < max; i++) {
      values.push(iter.next());
    }
    for (i = 0; i < 2; i++) {
      errors.push(iter.next());
    }

    readable.push('hello-0');
    readable.push('hello-1');
    readable.push('hello-2');

    const resolved = await Promise.all(values);

    resolved.forEach(common.mustCall(
      (item, i) => assert.strictEqual(item.value, 'hello-' + i), max));

    errors.forEach((promise) => {
      promise.catch(common.mustCall((err) => {
        assert.strictEqual(err.message, 'kaboom');
      }));
    });

    readable.destroy(new Error('kaboom'));
  })();

  await (async function() {
    console.log('call next() after error');
    const readable = new Readable({
      read() {}
    });
    const iterator = readable[Symbol.asyncIterator]();

    const err = new Error('kaboom');
    readable.destroy(new Error('kaboom'));
    await assert.rejects(iterator.next.bind(iterator), err);
  })();

  await (async function() {
    console.log('read object mode');
    const max = 42;
    let readed = 0;
    let received = 0;
    const readable = new Readable({
      objectMode: true,
      read() {
        this.push('hello');
        if (++readed === max) {
          this.push(null);
        }
      }
    });

    for await (const k of readable) {
      received++;
      assert.strictEqual(k, 'hello');
    }

    assert.strictEqual(readed, received);
  })();

  await (async function() {
    console.log('destroy sync');
    const readable = new Readable({
      objectMode: true,
      read() {
        this.destroy(new Error('kaboom from read'));
      }
    });

    let err;
    try {
      // eslint-disable-next-line no-unused-vars
      for await (const k of readable) {}
    } catch (e) {
      err = e;
    }
    assert.strictEqual(err.message, 'kaboom from read');
  })();

  await (async function() {
    console.log('destroy async');
    const readable = new Readable({
      objectMode: true,
      read() {
        if (!this.pushed) {
          this.push('hello');
          this.pushed = true;

          setImmediate(() => {
            this.destroy(new Error('kaboom'));
          });
        }
      }
    });

    let received = 0;

    let err = null;
    try {
      // eslint-disable-next-line no-unused-vars
      for await (const k of readable) {
        received++;
      }
    } catch (e) {
      err = e;
    }

    assert.strictEqual(err.message, 'kaboom');
    assert.strictEqual(received, 1);
  })();

  await (async function() {
    console.log('destroyed by throw');
    const readable = new Readable({
      objectMode: true,
      read() {
        this.push('hello');
      }
    });

    let err = null;
    try {
      for await (const k of readable) {
        assert.strictEqual(k, 'hello');
        throw new Error('kaboom');
      }
    } catch (e) {
      err = e;
    }

    assert.strictEqual(err.message, 'kaboom');
    assert.strictEqual(readable.destroyed, true);
  })();

  await (async function() {
    console.log('destroyed sync after push');
    const readable = new Readable({
      objectMode: true,
      read() {
        this.push('hello');
        this.destroy(new Error('kaboom'));
      }
    });

    let received = 0;

    let err = null;
    try {
      for await (const k of readable) {
        assert.strictEqual(k, 'hello');
        received++;
      }
    } catch (e) {
      err = e;
    }

    assert.strictEqual(err.message, 'kaboom');
    assert.strictEqual(received, 1);
  })();

  await (async function() {
    console.log('push async');
    const max = 42;
    let readed = 0;
    let received = 0;
    const readable = new Readable({
      objectMode: true,
      read() {
        setImmediate(() => {
          this.push('hello');
          if (++readed === max) {
            this.push(null);
          }
        });
      }
    });

    for await (const k of readable) {
      received++;
      assert.strictEqual(k, 'hello');
    }

    assert.strictEqual(readed, received);
  })();

  await (async function() {
    console.log('push binary async');
    const max = 42;
    let readed = 0;
    const readable = new Readable({
      read() {
        setImmediate(() => {
          this.push('hello');
          if (++readed === max) {
            this.push(null);
          }
        });
      }
    });

    let expected = '';
    readable.setEncoding('utf8');
    readable.pause();
    readable.on('data', (chunk) => {
      expected += chunk;
    });

    let data = '';
    for await (const k of readable) {
      data += k;
    }

    assert.strictEqual(data, expected);
  })();

  await (async function() {
    console.log('.next() on destroyed stream');
    const readable = new Readable({
      read() {
        // no-op
      }
    });

    readable.destroy();

    const { done } = await readable[Symbol.asyncIterator]().next();
    assert.strictEqual(done, true);
  })();

  await (async function() {
    console.log('.next() on pipelined stream');
    const readable = new Readable({
      read() {
        // no-op
      }
    });

    const passthrough = new PassThrough();
    const err = new Error('kaboom');
    pipeline(readable, passthrough, common.mustCall((e) => {
      assert.strictEqual(e, err);
    }));
    readable.destroy(err);
    try {
      await readable[Symbol.asyncIterator]().next();
    } catch (e) {
      assert.strictEqual(e, err);
    }
  })();

  await (async () => {
    console.log('iterating on an ended stream completes');
    const r = new Readable({
      objectMode: true,
      read() {
        this.push('asdf');
        this.push('hehe');
        this.push(null);
      }
    });
    // eslint-disable-next-line no-unused-vars
    for await (const a of r) {
    }
    // eslint-disable-next-line no-unused-vars
    for await (const b of r) {
    }
  })();

  await (async () => {
    console.log('destroy mid-stream does not error');
    const r = new Readable({
      objectMode: true,
      read() {
        this.push('asdf');
        this.push('hehe');
      }
    });

    // eslint-disable-next-line no-unused-vars
    for await (const a of r) {
      r.destroy(null);
    }
  })();

  await (async () => {
    console.log('all next promises must be resolved on end');
    const r = new Readable({
      objectMode: true,
      read() {
      }
    });

    const b = r[Symbol.asyncIterator]();
    const c = b.next();
    const d = b.next();
    r.push(null);
    assert.deepStrictEqual(await c, { done: true, value: undefined });
    assert.deepStrictEqual(await d, { done: true, value: undefined });
  })();

  await (async () => {
    console.log('all next promises must be resolved on destroy');
    const r = new Readable({
      objectMode: true,
      read() {
      }
    });

    const b = r[Symbol.asyncIterator]();
    const c = b.next();
    const d = b.next();
    r.destroy();
    assert.deepStrictEqual(await c, { done: true, value: undefined });
    assert.deepStrictEqual(await d, { done: true, value: undefined });
  })();

  await (async () => {
    console.log('all next promises must be resolved on destroy with error');
    const r = new Readable({
      objectMode: true,
      read() {
      }
    });

    const b = r[Symbol.asyncIterator]();
    const c = b.next();
    const d = b.next();
    const err = new Error('kaboom');
    r.destroy(err);

    await Promise.all([(async () => {
      let e;
      try {
        await c;
      } catch (_e) {
        e = _e;
      }
      assert.strictEqual(e, err);
    })(), (async () => {
      let e;
      try {
        await d;
      } catch (_e) {
        e = _e;
      }
      assert.strictEqual(e, err);
    })()]);
  })();
}

// to avoid missing some tests if a promise does not resolve
tests().then(common.mustCall());
