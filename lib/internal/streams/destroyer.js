'use strict';

const { once } = require('util');

module.exports = function destroy (stream, err, cb) {
  cb = once(cb);
  if (stream.destroy) {
    stream.destroy(err, (er) => {
      process.nextTick(cb, er);
    });
  } else if (stream.abort) {
    stream.abort();
  } else if (stream.end) {
    stream.end();
  } else {
    process.nextTick(cb, new Error('invalid stream'));
    return;
  }
  // Register event listener for streams without destroy(err, cb).
  stream
    .once('error', cb)
    .once('aborted', () => cb(new Error('aborted')))
    .once('close', () => {
      // Wait one tick in case 'error' is emitted after 'close'.
      process.nextTick(cb);
    });
}
