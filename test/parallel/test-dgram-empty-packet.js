'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var dgram = require('dgram');
var callbacks = 0;
var client;
var timer;

if (process.platform === 'darwin') {
  console.log('1..0 # Skipped: because of 17894467 Apple bug');
  return;
}

client = dgram.createSocket('udp4');

client.bind(common.PORT);

function callback() {
  callbacks++;
  if (callbacks == 2) {
    clearTimeout(timer);
    client.close();
  } else if (callbacks > 2) {
    throw new Error('the callbacks should be called only two times');
  }
}

client.on('message', function(buffer, bytes) {
  callback();
});

client.send(new Buffer(1), 0, 0, common.PORT, '127.0.0.1', function(err, len) {
  callback();
});

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
