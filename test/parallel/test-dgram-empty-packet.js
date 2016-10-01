'use strict';
const common = require('../common');
const dgram = require('dgram');

let callbacks = 0;
let timer;

if (common.isOSX) {
  common.skip('because of 17894467 Apple bug');
  return;
}

const client = dgram.createSocket('udp4');

client.bind(0, function() {

  function callback() {
    callbacks++;
    if (callbacks === 2) {
      clearTimeout(timer);
      client.close();
    } else if (callbacks > 2) {
      throw new Error('the callbacks should be called only two times');
    }
  }

  client.on('message', function(buffer, bytes) {
    callback();
  });

  const port = this.address().port;
  client.send(
    Buffer.allocUnsafe(1), 0, 0, port, '127.0.0.1', (err, len) => {
      callback();
    });

  timer = setTimeout(function() {
    throw new Error('Timeout');
  }, 200);
});
