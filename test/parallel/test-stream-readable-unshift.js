'use strict';

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');

{
  // Check that strings are saved as Buffer
  const readable = new Readable({ read() {} });

  const string = 'abc';

  readable.on('data', common.mustCall((chunk) => {
    assert(Buffer.isBuffer(chunk));
    assert.strictEqual(chunk.toString('utf8'), string);
  }, 1));

  readable.unshift(string);

}

{
  // Check that data goes at the beginning
  const readable = new Readable({ read() {} });
  const unshift = 'front';
  const push = 'back';

  const expected = [unshift, push];
  readable.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString('utf8'), expected.shift());
  }, 2));


  readable.push(push);
  readable.unshift(unshift);
}

{
  // Check that buffer is saved with correct encoding
  const readable = new Readable({ read() {} });

  const encoding = 'base64';
  const string = Buffer.from('abc').toString(encoding);

  readable.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk.toString(encoding), string);
  }, 1));

  readable.unshift(string, encoding);

}

{

  const streamEncoding = 'base64';

  function checkEncoding(readable) {

    // chunk encodings
    const encodings = ['utf8', 'binary', 'hex', 'base64'];
    const expected = [];

    readable.on('data', common.mustCall((chunk) => {
      const { encoding, string } = expected.pop();
      assert.strictEqual(chunk.toString(encoding), string);
    }, encodings.length));

    for (const encoding of encodings) {
      const string = 'abc';

      // If encoding is the same as the state.encoding the string is
      // saved as is
      const expect = encoding !== streamEncoding ?
        Buffer.from(string, encoding).toString(streamEncoding) : string;

      expected.push({ encoding, string: expect });

      readable.unshift(string, encoding);
    }
  }

  const r1 = new Readable({ read() {} });
  r1.setEncoding(streamEncoding);
  checkEncoding(r1);

  const r2 = new Readable({ read() {}, encoding: streamEncoding });
  checkEncoding(r2);

}

{
  // Both .push & .unshift should have the same behaviour
  // When setting an encoding, each chunk should be emitted with that encoding
  const encoding = 'base64';

  function checkEncoding(readable) {
    const string = 'abc';
    readable.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk, Buffer.from(string).toString(encoding));
    }, 2));

    readable.push(string);
    readable.unshift(string);
  }

  const r1 = new Readable({ read() {} });
  r1.setEncoding(encoding);
  checkEncoding(r1);

  const r2 = new Readable({ read() {}, encoding });
  checkEncoding(r2);

}

{
  // Check that ObjectMode works
  const readable = new Readable({ objectMode: true, read() {} });

  const chunks = ['a', 1, {}, []];

  readable.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, chunks.pop());
  }, chunks.length));

  for (const chunk of chunks) {
    readable.unshift(chunk);
  }
}

{

  // Should not throw: https://github.com/nodejs/node/issues/27192
  const highWaterMark = 50;
  class ArrayReader extends Readable {
    constructor(opt) {
      super({ highWaterMark });
      // The error happened only when pushing above hwm
      this.buffer = new Array(highWaterMark * 2).fill(0).map(String);
    }
    _read(size) {
      while (this.buffer.length) {
        const chunk = this.buffer.shift();
        if (!this.buffer.length) {
          this.push(chunk);
          this.push(null);
          return true;
        }
        if (!this.push(chunk))
          return;
      }
    }
  }

  function onRead() {
    while (null !== (stream.read())) {
      // Remove the 'readable' listener before unshifting
      stream.removeListener('readable', onRead);
      stream.unshift('a');
      stream.on('data', (chunk) => {
        console.log(chunk.length);
      });
      break;
    }
  }

  const stream = new ArrayReader();
  stream.once('readable', common.mustCall(onRead));
  stream.on('end', common.mustCall());

}
