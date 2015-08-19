'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var connected = false;
var secure = false;

process.on('exit', function() {
  assert(connected);
  assert(secure);
  console.log('ok');
});

var socket = net.connect(443, 'www.google.com', function() {
  connected = true;
  var secureSocket = tls.connect({ socket: socket }, function() {
    secure = true;
    secureSocket.destroy();
  });
});
