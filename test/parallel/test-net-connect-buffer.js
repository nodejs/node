'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var dataWritten = false;
var connectHappened = false;

var tcp = net.Server(function(s) {
  tcp.close();

  console.log('tcp server connection');

  var buf = '';
  s.on('data', function(d) {
    buf += d;
  });

  s.on('end', function() {
    console.error('SERVER: end', buf.toString());
    assert.equal(buf, "L'État, c'est moi");
    console.log('tcp socket disconnect');
    s.end();
  });

  s.on('error', function(e) {
    console.log('tcp server-side error: ' + e.message);
    process.exit(1);
  });
});

tcp.listen(0, function() {
  var socket = net.Stream({ highWaterMark: 0 });

  console.log('Connecting to socket ');

  socket.connect(this.address().port, function() {
    console.log('socket connected');
    connectHappened = true;
  });

  console.log('connecting = ' + socket.connecting);

  assert.equal('opening', socket.readyState);

  // Make sure that anything besides a buffer or a string throws.
  [null,
   true,
   false,
   undefined,
   1,
   1.0,
   1 / 0,
   +Infinity,
   -Infinity,
   [],
   {}
  ].forEach(function(v) {
    function f() {
      console.error('write', v);
      socket.write(v);
    }
    assert.throws(f, TypeError);
  });

  // Write a string that contains a multi-byte character sequence to test that
  // `bytesWritten` is incremented with the # of bytes, not # of characters.
  var a = "L'État, c'est ";
  var b = 'moi';

  // We're still connecting at this point so the datagram is first pushed onto
  // the connect queue. Make sure that it's not added to `bytesWritten` again
  // when the actual write happens.
  var r = socket.write(a, function(er) {
    console.error('write cb');
    dataWritten = true;
    assert.ok(connectHappened);
    console.error('socket.bytesWritten', socket.bytesWritten);
    //assert.equal(socket.bytesWritten, Buffer.from(a + b).length);
    console.error('data written');
  });
  console.error('socket.bytesWritten', socket.bytesWritten);
  console.error('write returned', r);

  assert.equal(socket.bytesWritten, Buffer.from(a).length);

  assert.equal(false, r);
  socket.end(b);

  assert.equal('opening', socket.readyState);
});

process.on('exit', function() {
  assert.ok(connectHappened);
  assert.ok(dataWritten);
});
