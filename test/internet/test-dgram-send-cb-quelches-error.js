'use strict';
var common = require('../common');
var mustCall = common.mustCall;
var assert = require('assert');
var dgram = require('dgram');
var dns = require('dns');

var socket = dgram.createSocket('udp4');
var buffer = Buffer.from('gary busey');

dns.setServers([]);

socket.once('error', onEvent);

// assert that:
// * callbacks act as "error" listeners if given.
// * error is never emitter for missing dns entries
//   if a callback that handles error is present
// * error is emitted if a callback with no argument is passed
socket.send(buffer, 0, buffer.length, 100,
            'dne.example.com', mustCall(callbackOnly));

function callbackOnly(err) {
  assert.ok(err);
  socket.removeListener('error', onEvent);
  socket.on('error', mustCall(onError));
  socket.send(buffer, 0, buffer.length, 100, 'dne.example.com');
}

function onEvent(err) {
  common.fail('Error should not be emitted if there is callback');
}

function onError(err) {
  assert.ok(err);
  socket.close();
}
