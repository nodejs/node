'use strict';
const common = require('../common');

const { Writable } = require('stream');

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      callback(null);
    },
    final(callback) {
      queueMicrotask(callback);
    }
  });
  w.end();
  w.destroy();

  w.on('prefinish', common.mustNotCall());
  w.on('finish', common.mustNotCall());
  w.on('close', common.mustCall());
}

{
  const w = new Writable({
    write(chunk, encoding, callback) {
      callback(null);
    },
    final(callback) {
      this.destroy();
      callback();
    }
  });

  w.on('error', common.mustNotCall());
  w.on('finish', common.mustNotCall());
  w.on('close', common.mustCall());
  w.end(common.expectsError({
    code: 'ERR_STREAM_DESTROYED',
    name: 'Error',
    message: 'Cannot call end after a stream was destroyed'
  }));
}
