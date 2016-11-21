'use strict';
const common = require('../common');

if (common.isOSX) {
  common.skip('because of 17894467 Apple bug');
  return;
}

const dgram = require('dgram');

const client = dgram.createSocket('udp4');

client.bind(0, common.mustCall(function() {

  function callback(firstArg) {
    // First time through, firstArg should be null.
    // Second time, it should be the buffer.
    if (firstArg instanceof Buffer)
      client.close();
  }

  client.on('message', common.mustCall(callback));

  const port = this.address().port;
  const buf = Buffer.alloc(1);
  client.send(buf, 0, 0, port, '127.0.0.1', common.mustCall(callback));
}));
