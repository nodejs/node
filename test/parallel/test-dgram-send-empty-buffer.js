'use strict';
const common = require('../common');
const dgram = require('dgram');

if (process.platform === 'darwin') {
  common.skip('because of 17894467 Apple bug');
  return;
}

const client = dgram.createSocket('udp4');

client.bind(0, function() {
  const port = this.address().port;

  client.on('message', common.mustCall(function onMessage(buffer, bytes) {
    clearTimeout(timer);
    client.close();
  }));

  const buf = new Buffer(0);
  client.send(buf, 0, 0, port, '127.0.0.1', function(err, len) { });

  const timer = setTimeout(function() {
    throw new Error('Timeout');
  }, common.platformTimeout(200));
});
