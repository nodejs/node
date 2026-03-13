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
