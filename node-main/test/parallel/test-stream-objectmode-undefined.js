'use strict';
const common = require('../common');
const assert = require('assert');
const { Readable, Writable, Transform } = require('stream');

{
  const stream = new Readable({
    objectMode: true,
    read: common.mustCall(() => {
      stream.push(undefined);
      stream.push(null);
    })
  });

  stream.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, undefined);
  }));
}

{
  const stream = new Writable({
    objectMode: true,
    write: common.mustCall((chunk) => {
      assert.strictEqual(chunk, undefined);
    })
  });

  stream.write(undefined);
}

{
  const stream = new Transform({
    objectMode: true,
    transform: common.mustCall((chunk) => {
      stream.push(chunk);
    })
  });

  stream.on('data', common.mustCall((chunk) => {
    assert.strictEqual(chunk, undefined);
  }));

  stream.write(undefined);
}
