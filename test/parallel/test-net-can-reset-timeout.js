'use strict';
var net = require('net');
var common = require('../common');
var assert = require('assert');

var timeoutCount = 0;

var server = net.createServer(function(stream) {
  stream.setTimeout(100);

  stream.resume();

  stream.on('timeout', function() {
    console.log('timeout');
    // try to reset the timeout.
    stream.write('WHAT.');
    // don't worry, the socket didn't *really* time out, we're just thinking
    // it did.
    timeoutCount += 1;
  });

  stream.on('end', function() {
    console.log('server side end');
    stream.end();
  });
});

server.listen(common.PORT, function() {
  var c = net.createConnection(common.PORT);

  c.on('data', function() {
    c.end();
  });

  c.on('end', function() {
    console.log('client side end');
    server.close();
  });
});


process.on('exit', function() {
  assert.equal(1, timeoutCount);
});
