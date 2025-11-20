'use strict';

const common = require('../common');
const assert = require('assert');
const { Duplex, Readable, Writable, pipeline, PassThrough } = require('stream');
const { ReadableStream, WritableStream } = require('stream/web');
const { Blob } = require('buffer');

{
  const d = Duplex.from({
    readable: new Readable({
      read() {
        this.push('asd');
        this.push(null);
      }
    })
  });
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}

{
  const d = Duplex.from(new Readable({
    read() {
      this.push('asd');
      this.push(null);
    }
  }));
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}

{
  let ret = '';
  const d = Duplex.from(new Writable({
    write(chunk, encoding, callback) {
      ret += chunk;
      callback();
    }
  }));
  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);
  d.end('asd');
  d.on('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    writable: new Writable({
      write(chunk, encoding, callback) {
        ret += chunk;
        callback();
      }
    })
  });
  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);
  d.end('asd');
  d.on('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    readable: new Readable({
      read() {
        this.push('asd');
        this.push(null);
      }
    }),
    writable: new Writable({
      write(chunk, encoding, callback) {
        ret += chunk;
        callback();
      }
    })
  });
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, true);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
  d.end('asd');
  d.once('finish', common.mustCall(function() {
    assert.strictEqual(d.writable, false);
    assert.strictEqual(ret, 'asd');
  }));
}

{
  const d = Duplex.from(Promise.resolve('asd'));
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);
  d.once('readable', common.mustCall(function() {
    assert.strictEqual(d.read().toString(), 'asd');
  }));
  d.once('end', common.mustCall(function() {
    assert.strictEqual(d.readable, false);
  }));
}

{
  // https://github.com/nodejs/node/issues/40497
  pipeline(
    ['abc\ndef\nghi'],
    Duplex.from(async function * (source) {
      let rest = '';
      for await (const chunk of source) {
        const lines = (rest + chunk.toString()).split('\n');
        rest = lines.pop();
        for (const line of lines) {
          yield line;
        }
      }
      yield rest;
    }),
    async function * (source) { // eslint-disable-line require-yield
      let ret = '';
      for await (const x of source) {
        ret += x;
      }
      assert.strictEqual(ret, 'abcdefghi');
    },
    common.mustSucceed(),
  );
}

// Ensure that isDuplexNodeStream was called
{
  const duplex = new Duplex();
  assert.strictEqual(Duplex.from(duplex), duplex);
}

// Ensure that Duplex.from works for blobs
{
  const blob = new Blob(['blob']);
  const expectedByteLength = blob.size;
  const duplex = Duplex.from(blob);
  duplex.on('data', common.mustCall((arrayBuffer) => {
    assert.strictEqual(arrayBuffer.byteLength, expectedByteLength);
  }));
}

// Ensure that given a promise rejection it emits an error
{
  const myErrorMessage = 'myCustomError';
  Duplex.from(Promise.reject(myErrorMessage))
    .on('error', common.mustCall((error) => {
      assert.strictEqual(error, myErrorMessage);
    }));
}

// Ensure that given a promise rejection on an async function it emits an error
{
  const myErrorMessage = 'myCustomError';
  async function asyncFn() {
    return Promise.reject(myErrorMessage);
  }

  Duplex.from(asyncFn)
    .on('error', common.mustCall((error) => {
      assert.strictEqual(error, myErrorMessage);
    }));
}

// Ensure that Duplex.from throws an Invalid return value when function is void
{
  assert.throws(() => Duplex.from(() => {}), {
    code: 'ERR_INVALID_RETURN_VALUE',
  });
}

// Ensure data if a sub object has a readable stream it's duplexified
{
  const msg = Buffer.from('hello');
  const duplex = Duplex.from({
    readable: Readable({
      read() {
        this.push(msg);
        this.push(null);
      }
    })
  }).on('data', common.mustCall((data) => {
    assert.strictEqual(data, msg);
  }));

  assert.strictEqual(duplex.writable, false);
}

// Ensure data if a sub object has a writable stream it's duplexified
{
  const msg = Buffer.from('hello');
  const duplex = Duplex.from({
    writable: Writable({
      write: common.mustCall((data) => {
        assert.strictEqual(data, msg);
      })
    })
  });

  duplex.write(msg);
  assert.strictEqual(duplex.readable, false);
}

// Ensure data if a sub object has a writable and readable stream it's duplexified
{
  const msg = Buffer.from('hello');

  const duplex = Duplex.from({
    readable: Readable({
      read() {
        this.push(msg);
        this.push(null);
      }
    }),
    writable: Writable({
      write: common.mustCall((data) => {
        assert.strictEqual(data, msg);
      })
    })
  });

  duplex.pipe(duplex)
    .on('data', common.mustCall((data) => {
      assert.strictEqual(data, msg);
      assert.strictEqual(duplex.readable, true);
      assert.strictEqual(duplex.writable, true);
    }))
    .on('end', common.mustCall());
}

// Ensure that given readable stream that throws an error it calls destroy
{
  const myErrorMessage = 'error!';
  const duplex = Duplex.from(Readable({
    read() {
      throw new Error(myErrorMessage);
    }
  }));
  duplex.on('error', common.mustCall((msg) => {
    assert.strictEqual(msg.message, myErrorMessage);
  }));
}

// Ensure that given writable stream that throws an error it calls destroy
{
  const myErrorMessage = 'error!';
  const duplex = Duplex.from(Writable({
    write(chunk, enc, cb) {
      cb(myErrorMessage);
    }
  }));

  duplex.on('error', common.mustCall((msg) => {
    assert.strictEqual(msg, myErrorMessage);
  }));

  duplex.write('test');
}

{
  const through = new PassThrough({ objectMode: true });

  let res = '';
  const d = Readable.from(['foo', 'bar'], { objectMode: true })
    .pipe(Duplex.from({
      writable: through,
      readable: through
    }));

  d.on('data', (data) => {
    d.pause();
    setImmediate(() => {
      d.resume();
    });
    res += data;
  }).on('end', common.mustCall(() => {
    assert.strictEqual(res, 'foobar');
  })).on('close', common.mustCall());
}

function makeATestReadableStream(value) {
  return new ReadableStream({
    start(controller) {
      controller.enqueue(value);
      controller.close();
    }
  });
}

function makeATestWritableStream(writeFunc) {
  return new WritableStream({
    write(chunk) {
      writeFunc(chunk);
    }
  });
}

{
  const d = Duplex.from({
    readable: makeATestReadableStream('foo'),
  });
  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);

  d.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'foo');
  }));

  d.on('end', common.mustCall(() => {
    assert.strictEqual(d.readable, false);
  }));
}

{
  const d = Duplex.from(makeATestReadableStream('foo'));

  assert.strictEqual(d.readable, true);
  assert.strictEqual(d.writable, false);

  d.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'foo');
  }));

  d.on('end', common.mustCall(() => {
    assert.strictEqual(d.readable, false);
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    writable: makeATestWritableStream((chunk) => ret += chunk),
  });

  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);

  d.end('foo');
  d.on('finish', common.mustCall(() => {
    assert.strictEqual(ret, 'foo');
    assert.strictEqual(d.writable, false);
  }));
}

{
  let ret = '';
  const d = Duplex.from(makeATestWritableStream((chunk) => ret += chunk));

  assert.strictEqual(d.readable, false);
  assert.strictEqual(d.writable, true);

  d.end('foo');
  d.on('finish', common.mustCall(() => {
    assert.strictEqual(ret, 'foo');
    assert.strictEqual(d.writable, false);
  }));
}

{
  let ret = '';
  const d = Duplex.from({
    readable: makeATestReadableStream('foo'),
    writable: makeATestWritableStream((chunk) => ret += chunk),
  });

  d.end('bar');

  d.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'foo');
  }));

  d.on('end', common.mustCall(() => {
    assert.strictEqual(d.readable, false);
  }));

  d.on('finish', common.mustCall(() => {
    assert.strictEqual(ret, 'bar');
    assert.strictEqual(d.writable, false);
  }));
}
