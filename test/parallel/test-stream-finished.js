'use strict';

const common = require('../common');
const {
  Writable,
  Readable,
  Transform,
  finished,
  Duplex,
  PassThrough
} = require('stream');
const assert = require('assert');
const EE = require('events');
const fs = require('fs');
const { promisify } = require('util');

{
  const rs = new Readable({
    read() {}
  });

  finished(rs, common.mustSucceed());

  rs.push(null);
  rs.resume();
}

{
  const ws = new Writable({
    write(data, enc, cb) {
      cb();
    }
  });

  finished(ws, common.mustSucceed());

  ws.end();
}

{
  const tr = new Transform({
    transform(data, enc, cb) {
      cb();
    }
  });

  let finish = false;
  let ended = false;

  tr.on('end', () => {
    ended = true;
  });

  tr.on('finish', () => {
    finish = true;
  });

  finished(tr, common.mustSucceed(() => {
    assert(finish);
    assert(ended);
  }));

  tr.end();
  tr.resume();
}

{
  const rs = fs.createReadStream(__filename);

  rs.resume();
  finished(rs, common.mustCall());
}

{
  const finishedPromise = promisify(finished);

  async function run() {
    const rs = fs.createReadStream(__filename);
    const done = common.mustCall();

    let ended = false;
    rs.resume();
    rs.on('end', () => {
      ended = true;
    });
    await finishedPromise(rs);
    assert(ended);
    done();
  }

  run();
}

{
  const rs = fs.createReadStream('file-does-not-exist');

  finished(rs, common.expectsError({
    code: 'ENOENT'
  }));
}

{
  const rs = new Readable();

  finished(rs, common.mustSucceed());

  rs.push(null);
  rs.emit('close'); // Should not trigger an error
  rs.resume();
}

{
  const rs = new Readable();

  finished(rs, common.mustCall((err) => {
    assert(err, 'premature close error');
  }));

  rs.emit('close'); // Should trigger error
  rs.push(null);
  rs.resume();
}

// Test faulty input values and options.
{
  const rs = new Readable({
    read() {}
  });

  assert.throws(
    () => finished(rs, 'foo'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /callback/
    }
  );
  assert.throws(
    () => finished(rs, 'foo', () => {}),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /options/
    }
  );
  assert.throws(
    () => finished(rs, {}, 'foo'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /callback/
    }
  );

  finished(rs, null, common.mustCall());

  rs.push(null);
  rs.resume();
}

// Test that calling returned function removes listeners
{
  const ws = new Writable({
    write(data, env, cb) {
      cb();
    }
  });
  const removeListener = finished(ws, common.mustNotCall());
  removeListener();
  ws.end();
}

{
  const rs = new Readable();
  const removeListeners = finished(rs, common.mustNotCall());
  removeListeners();

  rs.emit('close');
  rs.push(null);
  rs.resume();
}

{
  const streamLike = new EE();
  streamLike.readableEnded = true;
  streamLike.readable = true;
  finished(streamLike, common.mustCall());
  streamLike.emit('close');
}

{
  const writable = new Writable({ write() {} });
  writable.writable = false;
  writable.destroy();
  finished(writable, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}

{
  const readable = new Readable();
  readable.readable = false;
  readable.destroy();
  finished(readable, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      setImmediate(callback);
    }
  });
  finished(w, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  w.end('asd');
  w.destroy();
}

function testClosed(factory) {
  {
    // If already destroyed but finished is cancelled in same tick
    // don't invoke the callback,

    const s = factory();
    s.destroy();
    const dispose = finished(s, common.mustNotCall());
    dispose();
  }

  {
    // If already destroyed invoked callback.

    const s = factory();
    s.destroy();
    finished(s, common.mustCall());
  }

  {
    // Don't invoke until destroy has completed.

    let destroyed = false;
    const s = factory({
      destroy(err, cb) {
        setImmediate(() => {
          destroyed = true;
          cb();
        });
      }
    });
    s.destroy();
    finished(s, common.mustCall(() => {
      assert.strictEqual(destroyed, true);
    }));
  }

  {
    // Invoke callback even if close is inhibited.

    const s = factory({
      emitClose: false,
      destroy(err, cb) {
        cb();
        finished(s, common.mustCall());
      }
    });
    s.destroy();
  }

  {
    // Invoke with deep async.

    const s = factory({
      destroy(err, cb) {
        setImmediate(() => {
          cb();
          setImmediate(() => {
            finished(s, common.mustCall());
          });
        });
      }
    });
    s.destroy();
  }
}

testClosed((opts) => new Readable({ ...opts }));
testClosed((opts) => new Writable({ write() {}, ...opts }));

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      cb();
    },
    autoDestroy: false
  });
  w.end('asd');
  process.nextTick(() => {
    finished(w, common.mustCall());
  });
}

{
  const w = new Writable({
    write(chunk, encoding, cb) {
      cb(new Error());
    },
    autoDestroy: false
  });
  w.write('asd');
  w.on('error', common.mustCall(() => {
    finished(w, common.mustCall());
  }));
}

{
  const r = new Readable({
    autoDestroy: false
  });
  r.push(null);
  r.resume();
  r.on('end', common.mustCall(() => {
    finished(r, common.mustCall());
  }));
}

{
  const rs = fs.createReadStream(__filename, { autoClose: false });
  rs.resume();
  rs.on('close', common.mustNotCall());
  rs.on('end', common.mustCall(() => {
    finished(rs, common.mustCall());
  }));
}

{
  const d = new EE();
  d._writableState = {};
  d._writableState.finished = true;
  finished(d, { readable: false, writable: true }, common.mustCall((err) => {
    assert.strictEqual(err, undefined);
  }));
  d._writableState.errored = true;
  d.emit('close');
}

{
  const r = new Readable();
  finished(r, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  r.push('asd');
  r.push(null);
  r.destroy();
}

{
  const d = new Duplex({
    final(cb) { }, // Never close writable side for test purpose
    read() {
      this.push(null);
    }
  });

  d.on('end', common.mustCall());

  finished(d, { readable: true, writable: false }, common.mustCall());

  d.end();
  d.resume();
}

{
  const d = new Duplex({
    final(cb) { }, // Never close writable side for test purpose
    read() {
      this.push(null);
    }
  });

  d.on('end', common.mustCall());

  d.end();
  finished(d, { readable: true, writable: false }, common.mustCall());

  d.resume();
}

{
  // Test for compat for e.g. fd-slicer which implements
  // non standard destroy behavior which might not emit
  // 'close'.
  const r = new Readable();
  finished(r, common.mustCall());
  r.resume();
  r.push('asd');
  r.destroyed = true;
  r.push(null);
}

{
  // Regression https://github.com/nodejs/node/issues/33130
  const response = new PassThrough();

  class HelloWorld extends Duplex {
    constructor(response) {
      super({
        autoDestroy: false
      });

      this.response = response;
      this.readMore = false;

      response.once('end', () => {
        this.push(null);
      });

      response.on('readable', () => {
        if (this.readMore) {
          this._read();
        }
      });
    }

    _read() {
      const { response } = this;

      this.readMore = true;

      if (response.readableLength) {
        this.readMore = false;
      }

      let data;
      while ((data = response.read()) !== null) {
        this.push(data);
      }
    }
  }

  const instance = new HelloWorld(response);
  instance.setEncoding('utf8');
  instance.end();

  (async () => {
    await EE.once(instance, 'finish');

    setImmediate(() => {
      response.write('chunk 1');
      response.write('chunk 2');
      response.write('chunk 3');
      response.end();
    });

    let res = '';
    for await (const data of instance) {
      res += data;
    }

    assert.strictEqual(res, 'chunk 1chunk 2chunk 3');
  })().then(common.mustCall());
}

{
  const p = new PassThrough();
  p.end();
  finished(p, common.mustNotCall());
}

{
  const p = new PassThrough();
  p.end();
  p.on('finish', common.mustCall(() => {
    finished(p, common.mustNotCall());
  }));
}

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      process.nextTick(callback);
    }
  });
  w.aborted = false;
  w.end();
  let closed = false;
  w.on('finish', () => {
    assert.strictEqual(closed, false);
    w.emit('aborted');
  });
  w.on('close', common.mustCall(() => {
    closed = true;
  }));

  finished(w, common.mustCall(() => {
    assert.strictEqual(closed, true);
  }));
}
