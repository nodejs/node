'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

common.crashOnUnhandledRejection();

async function tests() {
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
}

// to avoid missing some tests if a promise does not resolve
tests().then(common.mustCall());
