'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Readable, Transform, Writable } = require('node:stream');

{
  const objectReadable = Readable.from([
    { hello: 'hello' },
    { world: 'world' },
  ]);

  const passThrough = new Transform({
    transform(chunk, _encoding, cb) {
      this.push(chunk);
      cb(null);
    },
  });

  passThrough.on('error', common.mustCall());

  objectReadable.pipe(passThrough);

  assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of passThrough);
  }, /ERR_INVALID_ARG_TYPE/).then(common.mustCall());
}

{
  const stringReadable = Readable.from(['hello', 'world']);

  const passThrough = new Transform({
    transform(chunk, _encoding, cb) {
      this.push(chunk);
      throw new Error('something went wrong');
    },
  });

  passThrough.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'something went wrong');
  }));

  stringReadable.pipe(passThrough);
}

{
  const binaryData = Buffer.from('binary data');

  const binaryReadable = new Readable({
    read() {
      this.push(binaryData);
      this.push(null);
    }
  });

  const binaryWritable = new Writable({
    write(chunk, _encoding, cb) {
      throw new Error('something went wrong');
    }
  });

  binaryWritable.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'something went wrong');
  }));
  binaryReadable.pipe(binaryWritable);
}
