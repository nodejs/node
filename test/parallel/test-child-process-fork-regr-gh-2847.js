'use strict';

const common = require('../common');
const assert = require('assert');

const cluster = require('cluster');
const net = require('net');
const util = require('util');

var connectcount = 0;
var sendcount = 0;

if (!cluster.isMaster) {
  // Exit on first received handle to leave the queue non-empty in master
  process.on('message', function() {
    process.exit(1);
  });
  return;
}

var server = net.createServer(function(s) {
  setTimeout(function() {
    s.destroy();
  }, 100);
}).listen(common.PORT, function() {
  var worker = cluster.fork();

  function send(callback) {
    var s = net.connect(common.PORT, function() {
      worker.send({}, s, callback);
      connectcount++;
    });

    // Errors can happen if the connections
    // are still happening while the server has been closed.
    // This can happen depending on how the messages are
    // bundled into packets. If they all make it into the first
    // one then no errors will occur, otherwise the server
    // may have been closed by the time the later ones make
    // it to the server side.
    // We ignore any errors that occur after some connections
    // get through
    s.on('error', function(err) {
      if (connectcount < 3)
        console.log(err);
    });
  }

  worker.process.once('close', common.mustCall(function() {
    // Otherwise the crash on `_channel.fd` access may happen
    assert(worker.process._channel === null);
    server.close();
  }));

  // Queue up several handles, to make `process.disconnect()` wait
  for (var i = 0; i < 100; i++)
    send(function(err) {
      if (err && sendcount < 3)
        console.log(err);
      sendcount++;
    });
});
