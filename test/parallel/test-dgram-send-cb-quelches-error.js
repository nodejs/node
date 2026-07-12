'use strict';
const common = require('../common');
const mustCall = common.mustCall;
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');

const socket = dgram.createSocket('udp4');
const buffer = Buffer.from('gary busey');

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
  socket.send(buffer, 0, buffer.length, 100, 'dne.invalid');
}

function onEvent(err) {
  assert.fail(`Error should not be emitted if there is callback: ${err}`);
}

function onError(err) {
  assert.ok(err);
  socket.close();
}
