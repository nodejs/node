'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const mockError = new Error('mock DNS error');

function getSocket(callback) {
  const socket = dgram.createSocket('udp4');

  socket.on('message', common.mustNotCall('Should not receive any messages.'));
  socket.bind(common.mustCall(() => {
    socket._handle.lookup = function(address, callback) {
      process.nextTick(callback, mockError);
    };

    callback(socket);
  }));
  return socket;
}

getSocket((socket) => {
  socket.on('error', common.mustCall((err) => {
    socket.close();
    assert.strictEqual(err, mockError);
  }));

  socket.send('foo', socket.address().port, 'localhost');
});

getSocket((socket) => {
  const callback = common.mustCall((err) => {
    socket.close();
    assert.strictEqual(err, mockError);
  });

  socket.send('foo', socket.address().port, 'localhost', callback);
});
