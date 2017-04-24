'use strict';
// Ensure that if a dgram socket is closed before the sendQueue is drained
// will not crash

const common = require('../common');
const dgram = require('dgram');

const buf = Buffer.alloc(1024, 42);

const socket = dgram.createSocket('udp4');

socket.on('listening', function() {
  socket.close();
});

// get a random port for send
const portGetter = dgram.createSocket('udp4')
  .bind(0, 'localhost', common.mustCall(() => {
    // adds a listener to 'listening' to send the data when
    // the socket is available
    socket.send(buf, 0, buf.length,
                portGetter.address().port,
                portGetter.address().address);

    portGetter.close();
  }));
