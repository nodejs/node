'use strict';
const common = require('../common');
const dgram = require('dgram');

if (process.platform === 'darwin') {
  console.log('1..0 # Skipped: because of 17894467 Apple bug');
  return;
}

const client = dgram.createSocket('udp4');

client.bind(common.PORT);

client.on('message', function(buffer, bytes) {
  clearTimeout(timer);
  client.close();
});

const buf = new Buffer(0);
client.send(buf, 0, 0, common.PORT, '127.0.0.1', function(err, len) { });

const timer = setTimeout(function() {
  throw new Error('Timeout');
}, common.platformTimeout(200));
