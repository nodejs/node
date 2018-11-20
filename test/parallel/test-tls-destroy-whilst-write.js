'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const stream = require('stream');

const delay = new stream.Duplex({
  read: function read() {
  },
  write: function write(data, enc, cb) {
    console.log('pending');
    setImmediate(function() {
      console.log('done');
      cb();
    });
  }
});

const secure = tls.connect({
  socket: delay
});
setImmediate(function() {
  secure.destroy();
});
