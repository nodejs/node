'use strict';
const common = require('../common');
const { Writable } = require('stream');

// Don't emit 'drain' if ended

const w = new Writable({
  write(data, enc, cb) {
    process.nextTick(cb);
  },
  highWaterMark: 1
});

w.on('drain', common.mustNotCall());
w.write('asd');
w.end();
