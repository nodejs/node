'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var dgram = require('dgram');
var callbacks = 0;
var client, timer, buf;

if (process.platform === 'darwin') {
  console.log('1..0 # Skipped: because of 17894467 Apple bug');
  return;
}


client = dgram.createSocket('udp4');

client.bind(common.PORT);

client.on('message', function(buffer, bytes) {
  clearTimeout(timer);
  client.close();
});

buf = new Buffer(0);
client.send(buf, 0, 0, common.PORT, '127.0.0.1', function(err, len) { });

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
