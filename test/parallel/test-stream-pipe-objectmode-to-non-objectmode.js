'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Readable, Transform } = require('node:stream');

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
  }, /ERR_STREAM_INCOMPATIBLE_OBJECT_MODE/).then(common.mustCall());
}
