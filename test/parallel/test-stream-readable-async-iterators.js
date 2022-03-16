'use strict';

const common = require('../common');
const {
  Stream,
  Readable,
  Transform,
  PassThrough,
  pipeline
} = require('stream');
const assert = require('assert');
const http = require('http');
const fs = require('fs');

async function tests() {
  {
    // v1 stream

    const stream = new Stream();
    stream.destroy = common.mustCall();
    process.nextTick(() => {
      stream.emit('data', 'hello');
      stream.emit('data', 'world');
      stream.emit('end');
    });

    let res = '';
    stream[Symbol.asyncIterator] = Readable.prototype[Symbol.asyncIterator];
    for await (const d of stream) {
      res += d;
    }
    assert.strictEqual(res, 'helloworld');
  }

  {
    // v1 stream error

    const stream = new Stream();
    stream.close = common.mustCall();
    process.nextTick(() => {
      stream.emit('data', 0);
      stream.emit('data', 1);
      stream.emit('error', new Error('asd'));
    });

    const iter = Readable.prototype[Symbol.asyncIterator].call(stream);
    await iter.next()
      .then(common.mustNotCall())
      .catch(common.mustCall((err) => {
        assert.strictEqual(err.message, 'asd');
      }));
  }

  {
    // Non standard stream cleanup

    const readable = new Readable({ autoDestroy: false, read() {} });
    readable.push('asd');
    readable.push('asd');
    readable.destroy = null;
    readable.close = common.mustCall(() => {
      readable.emit('close');
    });

    await (async () => {
      for await (const d of readable) { // eslint-disable-line no-unused-vars
        return;
      }
    })();
  }

  {
    const readable = new Readable({ objectMode: true, read() {} });
    readable.push(0);
    readable.push(1);
    readable.push(null);

    const iter = readable[Symbol.asyncIterator]();
    assert.strictEqual((await iter.next()).value, 0);
    for await (const d of iter) {
      assert.strictEqual(d, 1);
    }
  }

  {
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
  }

  {
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
  }

  {
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

    errors.slice(0, 1).forEach((promise) => {
      promise.catch(common.mustCall((err) => {
        assert.strictEqual(err.message, 'kaboom');
      }));
    });

    errors.slice(1).forEach((promise) => {
      promise.then(common.mustCall(({ done, value }) => {
        assert.strictEqual(done, true);
        assert.strictEqual(value, undefined);
      }));
    });

    readable.destroy(new Error('kaboom'));
  }

  {
    console.log('call next() after error');
    const readable = new Readable({
      read() {}
    });
    const iterator = readable[Symbol.asyncIterator]();

    const err = new Error('kaboom');
    readable.destroy(err);
    await assert.rejects(iterator.next.bind(iterator), err);
  }

  {
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
  }

  {
    console.log('destroy sync');
    const readable = new Readable({
      objectMode: true,
      read() {
        this.destroy(new Error('kaboom from read'));
      }
    });

    let err;
    try {
      // eslint-disable-next-line no-unused-vars, no-empty
      for await (const k of readable) { }
    } catch (e) {
      err = e;
    }
    assert.strictEqual(err.message, 'kaboom from read');
  }

  {
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
  }

  {
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
  }

  {
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
  }

  {
    console.log('destroyed will not deadlock');
    const readable = new Readable();
    readable.destroy();
    process.nextTick(async () => {
      readable.on('close', common.mustNotCall());
      let received = 0;
      let err = null;
      try {
        for await (const k of readable) {
          // Just make linting pass. This should never run.
          assert.strictEqual(k, 'hello');
          received++;
        }
      } catch (_err) {
        err = _err;
      }
      assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
      assert.strictEqual(received, 0);
    });
  }

  {
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
  }

  {
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
  }

  {
    console.log('.next() on destroyed stream');
    const readable = new Readable({
      read() {
        // no-op
      }
    });

    readable.destroy();

    const it = await readable[Symbol.asyncIterator]();
    const next = it.next();
    next
      .then(common.mustNotCall())
      .catch(common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
      }));
  }

  {
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
    await assert.rejects(
      readable[Symbol.asyncIterator]().next(),
      (e) => {
        assert.strictEqual(e, err);
        return true;
      }
    );
  }

  {
    console.log('iterating on an ended stream completes');
    const r = new Readable({
      objectMode: true,
      read() {
        this.push('asdf');
        this.push('hehe');
        this.push(null);
      }
    });
    // eslint-disable-next-line no-unused-vars, no-empty
    for await (const a of r) { }
    // eslint-disable-next-line no-unused-vars, no-empty
    for await (const b of r) { }
  }

  {
    console.log('destroy mid-stream errors');
    const r = new Readable({
      objectMode: true,
      read() {
        this.push('asdf');
        this.push('hehe');
      }
    });

    let err = null;
    try {
      // eslint-disable-next-line no-unused-vars
      for await (const a of r) {
        r.destroy(null);
      }
    } catch (_err) {
      err = _err;
    }
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }

  {
    console.log('readable side of a transform stream pushes null');
    const transform = new Transform({
      objectMode: true,
      transform: (chunk, enc, cb) => { cb(null, chunk); }
    });
    transform.push(0);
    transform.push(1);
    process.nextTick(() => {
      transform.push(null);
    });

    const mustReach = [ common.mustCall(), common.mustCall() ];

    const iter = transform[Symbol.asyncIterator]();
    assert.strictEqual((await iter.next()).value, 0);

    for await (const d of iter) {
      assert.strictEqual(d, 1);
      mustReach[0]();
    }
    mustReach[1]();
  }

  {
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
  }

  {
    console.log('all next promises must be rejected on destroy');
    const r = new Readable({
      objectMode: true,
      read() {
      }
    });

    const b = r[Symbol.asyncIterator]();
    const c = b.next();
    const d = b.next();
    r.destroy();
    c
      .then(common.mustNotCall())
      .catch(common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
      }));
    assert.deepStrictEqual(await d, { done: true, value: undefined });
  }

  {
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
      let x;
      try {
        x = await d;
      } catch (_e) {
        e = _e;
      }
      assert.strictEqual(e, undefined);
      assert.strictEqual(x.done, true);
      assert.strictEqual(x.value, undefined);
    })()]);
  }

  {
    const _err = new Error('asd');
    const r = new Readable({
      read() {
      },
      destroy(err, callback) {
        setTimeout(() => callback(_err), 1);
      }
    });

    r.destroy();
    const it = r[Symbol.asyncIterator]();
    it.next().catch(common.mustCall((err) => {
      assert.strictEqual(err, _err);
    }));
  }

  {
    // Don't destroy if no auto destroy.
    // https://github.com/nodejs/node/issues/35116

    const r = new Readable({
      autoDestroy: false,
      read() {
        this.push('asd');
        this.push(null);
      }
    });

    for await (const chunk of r) { } // eslint-disable-line no-unused-vars, no-empty
    assert.strictEqual(r.destroyed, false);
  }

  {
    // Destroy if no auto destroy and premature break.
    // https://github.com/nodejs/node/pull/35122/files#r485678318

    const r = new Readable({
      autoDestroy: false,
      read() {
        this.push('asd');
      }
    });

    for await (const chunk of r) { // eslint-disable-line no-unused-vars
      break;
    }
    assert.strictEqual(r.destroyed, true);
  }

  {
    // Don't destroy before 'end'.

    const r = new Readable({
      read() {
        this.push('asd');
        this.push(null);
      }
    }).on('end', () => {
      assert.strictEqual(r.destroyed, false);
    });

    for await (const chunk of r) { } // eslint-disable-line no-unused-vars, no-empty
    assert.strictEqual(r.destroyed, true);
  }
}

{
  // AsyncIterator return should end even when destroy
  // does not implement the callback API.

  const r = new Readable({
    objectMode: true,
    read() {
    }
  });

  const originalDestroy = r.destroy;
  r.destroy = (err) => {
    originalDestroy.call(r, err);
  };
  const it = r[Symbol.asyncIterator]();
  const p = it.return();
  r.push(null);
  p.then(common.mustCall());
}


{
  // AsyncIterator return should not error with
  // premature close.

  const r = new Readable({
    objectMode: true,
    read() {
    }
  });

  const originalDestroy = r.destroy;
  r.destroy = (err) => {
    originalDestroy.call(r, err);
  };
  const it = r[Symbol.asyncIterator]();
  const p = it.return();
  r.emit('close');
  p.then(common.mustCall()).catch(common.mustNotCall());
}

{
  // AsyncIterator should not finish correctly if destroyed.

  const r = new Readable({
    objectMode: true,
    read() {
    }
  });

  r.destroy();
  r.on('close', () => {
    const it = r[Symbol.asyncIterator]();
    const next = it.next();
    next
      .then(common.mustNotCall())
      .catch(common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
      }));
  });
}

{
  // AsyncIterator should throw if prematurely closed
  // before end has been emitted.
  (async function() {
    const readable = fs.createReadStream(__filename);

    try {
      // eslint-disable-next-line no-unused-vars
      for await (const chunk of readable) {
        readable.close();
      }

      assert.fail('should have thrown');
    } catch (err) {
      assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
    }

    assert.ok(readable.destroyed);
  })().then(common.mustCall());
}

// AsyncIterator non-destroying iterator
{
  function createReadable() {
    return Readable.from((async function* () {
      await Promise.resolve();
      yield 5;
      await Promise.resolve();
      yield 7;
      await Promise.resolve();
    })());
  }

  // Check explicit destroying on return
  (async function() {
    const readable = createReadable();
    for await (const chunk of readable.iterator({ destroyOnReturn: true })) {
      assert.strictEqual(chunk, 5);
      break;
    }

    assert.ok(readable.destroyed);
  })().then(common.mustCall());

  // Check explicit non-destroy with return true
  (async function() {
    const readable = createReadable();
    const opts = { destroyOnReturn: false };
    for await (const chunk of readable.iterator(opts)) {
      assert.strictEqual(chunk, 5);
      break;
    }

    assert.ok(!readable.destroyed);

    for await (const chunk of readable.iterator(opts)) {
      assert.strictEqual(chunk, 7);
    }

    assert.ok(readable.destroyed);
  })().then(common.mustCall());

  // Check non-object options.
  {
    const readable = createReadable();
    assert.throws(
      () => readable.iterator(42),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "options" argument must be of type object. Received ' +
                 'type number (42)',
      }
    );
  }

  // Check for dangling listeners
  (async function() {
    const readable = createReadable();
    const opts = { destroyOnReturn: false };
    while (readable.readable) {
      // eslint-disable-next-line no-unused-vars
      for await (const chunk of readable.iterator(opts)) {
        break;
      }
    }

    assert.deepStrictEqual(readable.eventNames(), []);
  })().then(common.mustCall());
}

{
  let _req;
  const server = http.createServer((request, response) => {
    response.statusCode = 404;
    response.write('never ends');
  });

  server.listen(() => {
    _req = http.request(`http://localhost:${server.address().port}`)
      .on('response', common.mustCall(async (res) => {
        setTimeout(() => {
          _req.destroy(new Error('something happened'));
        }, 100);

        res.on('error', common.mustCall());

        let _err;
        try {
          // eslint-disable-next-line no-unused-vars, no-empty
          for await (const chunk of res) { }
        } catch (err) {
          _err = err;
        }

        assert.strictEqual(_err.code, 'ECONNRESET');
        server.close();
      }))
      .on('error', common.mustCall())
      .end();
  });
}

{
  async function getParsedBody(request) {
    let body = '';

    for await (const data of request) {
      body += data;
    }

    try {
      return JSON.parse(body);
    } catch {
      return {};
    }
  }

  const str = JSON.stringify({ asd: true });
  const server = http.createServer(async (request, response) => {
    const body = await getParsedBody(request);
    response.statusCode = 200;
    assert.strictEqual(JSON.stringify(body), str);
    response.end(JSON.stringify(body));
  }).listen(() => {
    http
      .request({
        method: 'POST',
        hostname: 'localhost',
        port: server.address().port,
      })
      .end(str)
      .on('response', async (res) => {
        let body = '';
        for await (const chunk of res) {
          body += chunk;
        }
        assert.strictEqual(body, str);
        server.close();
      });
  });
}

// To avoid missing some tests if a promise does not resolve
tests().then(common.mustCall());
