var common = require('../common');
var assert = require('assert');

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
