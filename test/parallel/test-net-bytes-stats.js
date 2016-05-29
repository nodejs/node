'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var bytesRead = 0;
var bytesWritten = 0;
var count = 0;

var tcp = net.Server(function(s) {
  console.log('tcp server connection');

  // trigger old mode.
  s.resume();

  s.on('end', function() {
    bytesRead += s.bytesRead;
    console.log('tcp socket disconnect #' + count);
  });
});

tcp.listen(0, function doTest() {
  console.error('listening');
  var socket = net.createConnection(this.address().port);

  socket.on('connect', function() {
    count++;
    console.error('CLIENT connect #%d', count);

    socket.write('foo', function() {
      console.error('CLIENT: write cb');
      socket.end('bar');
    });
  });

  socket.on('finish', function() {
    bytesWritten += socket.bytesWritten;
    console.error('CLIENT end event #%d', count);
  });

  socket.on('close', function() {
    console.error('CLIENT close event #%d', count);
    console.log('Bytes read: ' + bytesRead);
    console.log('Bytes written: ' + bytesWritten);
    if (count < 2) {
      console.error('RECONNECTING');
      socket.connect(tcp.address().port);
    } else {
      tcp.close();
    }
  });
});

process.on('exit', function() {
  assert.equal(bytesRead, 12);
  assert.equal(bytesWritten, 12);
});
