'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const Duplexify = require('internal/streams/duplexify');
const {
  Blob
} = require('internal/blob');
const {
  Duplex,
  Readable,
  Writable
} = require('stream');

// Ensure that isDuplexNodeStream was called
{
  const duplex = new Duplex();
  assert.deepStrictEqual(Duplexify(duplex), duplex);
}

// Ensure that Duplexify works for blobs
{
  const blob = new Blob(['blob']);
  const expecteByteLength = blob.size;
  const duplex = Duplexify(blob);
  duplex.on('data', common.mustCall((arrayBuffer) => {
    assert.deepStrictEqual(arrayBuffer.byteLength, expecteByteLength);
  }));
}

// Ensure that given a promise rejection it emits an error
{
  const myErrorMessage = 'myCustomError';
  Duplexify(Promise.reject(myErrorMessage))
    .on('error', common.mustCall((error) => {
      assert.deepStrictEqual(error, myErrorMessage);
    }));
}

// Ensure that given a promise rejection on an async function it emits an error
{
  const myErrorMessage = 'myCustomError';
  async function asyncFn() {
    return Promise.reject(myErrorMessage);
  }

  Duplexify(asyncFn)
    .on('error', common.mustCall((error) => {
      assert.deepStrictEqual(error, myErrorMessage);
    }));
}

// Ensure that Duplexify throws an Invalid return value when function is void
{
  assert.throws(() => Duplexify(() => {}), {
    code: 'ERR_INVALID_RETURN_VALUE',
  });
}

// Ensure data if a sub object has a readable stream it's duplexified
{
  const msg = Buffer.from('hello');
  const duplex = Duplexify({
    readable: Readable({
      read() {
        this.push(msg);
        this.push(null);
      }
    })
  }).on('data', common.mustCall((data) => {
    assert.deepStrictEqual(data, msg);
  }));

  assert.strictEqual(duplex.writable, false);
}

// Ensure data if a sub object has a writable stream it's duplexified
{
  const msg = Buffer.from('hello');
  const duplex = Duplexify({
    writable: Writable({
      write: common.mustCall((data) => {
        assert.deepStrictEqual(data, msg);
      })
    })
  });

  duplex.write(msg);
  assert.strictEqual(duplex.readable, false);
}

// Ensure data if a sub object has a writable and readable stream it's duplexified
{
  const msg = Buffer.from('hello');

  const duplex = Duplexify({
    readable: Readable({
      read() {
        this.push(msg);
        this.push(null);
      }
    }),
    writable: Writable({
      write: common.mustCall((data) => {
        assert.deepStrictEqual(data, msg);
      })
    })
  });

  duplex.pipe(duplex)
    .on('data', common.mustCall((data) => {
      assert.deepStrictEqual(data, msg);
      assert.strictEqual(duplex.readable, true);
      assert.strictEqual(duplex.writable, true);
    }))
    .on('end', common.mustCall());

}

// Ensure that given readable stream that throws an error it calls destroy
{
  const myErrorMessage = 'error!';
  const duplex = Duplexify(Readable({
    read() {
      throw new Error(myErrorMessage);
    }
  }));
  duplex.on('error', common.mustCall((msg) => {
    assert.deepStrictEqual(msg.message, myErrorMessage);
  }));
}

// Ensure that given writable stream that throws an error it calls destroy
{
  const myErrorMessage = 'error!';
  const duplex = Duplexify(Writable({
    write(chunk, enc, cb) {
      cb(myErrorMessage);
    }
  }));

  duplex.on('error', common.mustCall((msg) => {
    assert.deepStrictEqual(msg, myErrorMessage);
  }));

  duplex.write('test');
}
