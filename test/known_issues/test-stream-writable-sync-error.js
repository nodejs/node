'use strict';
const common = require('../common');

// Tests for the regression in _stream_writable discussed in
// https://github.com/nodejs/node/pull/31756

// Specifically, when a write callback is invoked synchronously
// with an error, and autoDestroy is not being used, the error
// should still be emitted on nextTick.

const { Writable } = require('stream');

class MyStream extends Writable {
  #cb = undefined;

  constructor() {
    super({ autoDestroy: false });
  }

  _write(_, __, cb) {
    this.#cb = cb;
  }

  close() {
    // Synchronously invoke the callback with an error.
    this.#cb(new Error('foo'));
  }
}

const stream = new MyStream();

const mustError = common.mustCall(2);

stream.write('test', () => {});

// Both error callbacks should be invoked.

stream.on('error', mustError);

stream.close();

// Without the fix in #31756, the error handler
// added after the call to close will not be invoked.
stream.on('error', mustError);
