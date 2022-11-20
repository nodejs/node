'use strict';

const common = require('../common');
const { pipeline, Duplex, PassThrough, Readable } = require('stream');
const assert = require('assert');

{
  // Call pipeline error function with ERR_STREAM_PREMATURE_CLOSE error if dst close before src
  const remote = new PassThrough();
  const local = new Duplex({
    read() {
    },
    write(chunk, enc, callback) {
      callback();
    },
  });

  pipeline(remote, local, remote, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));

  setImmediate(() => {
    remote.end();
  });
}


{
  // Must call when last stream is Duplex from function
  pipeline(
    Readable.from(['a', 'b', 'c', 'd']),
    Duplex.from(async function*(stream) {
      for await (const chunk of stream) {
        yield chunk;
      }
    }),
    common.mustSucceed(),
  ).on('error', common.mustNotCall());
}
