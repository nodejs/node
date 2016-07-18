'use strict';
const common = require('../common');
var assert = require('assert');
var fork = require('child_process').fork;
var net = require('net');

// child
if (process.argv[2] === 'child') {

  // Check that the 'disconnect' event is deferred to the next event loop tick.
  var disconnect = process.disconnect;
  process.disconnect = function() {
    disconnect.apply(this, arguments);
    // If the event is emitted synchronously, we're too late by now.
    process.once('disconnect', common.mustCall(disconnectIsNotAsync));
    // The funky function name makes it show up legible in mustCall errors.
    function disconnectIsNotAsync() {}
  };

  var server = net.createServer();

  server.on('connection', function(socket) {

    socket.resume();

    process.on('disconnect', function() {
      socket.end((process.connected).toString());
    });

    // when the socket is closed, we will close the server
    // allowing the process to self terminate
    socket.on('end', function() {
      server.close();
    });

    socket.write('ready');
  });

  // when the server is ready tell parent
  server.on('listening', function() {
    process.send({ msg: 'ready', port: server.address().port });
  });

  server.listen(0);

} else {
  // testcase
  var child = fork(process.argv[1], ['child']);

  var childFlag = false;
  var parentFlag = false;

  // when calling .disconnect the event should emit
  // and the disconnected flag should be true.
  child.on('disconnect', common.mustCall(function() {
    parentFlag = child.connected;
  }));

  // the process should also self terminate without using signals
  child.on('exit', common.mustCall(function() {}));

  // when child is listening
  child.on('message', function(obj) {
    if (obj && obj.msg === 'ready') {

      // connect to child using TCP to know if disconnect was emitted
      var socket = net.connect(obj.port);

      socket.on('data', function(data) {
        data = data.toString();

        // ready to be disconnected
        if (data === 'ready') {
          child.disconnect();
          assert.throws(child.disconnect.bind(child), Error);
          return;
        }

        // disconnect is emitted
        childFlag = (data === 'true');
      });

    }
  });

  process.on('exit', function() {
    assert.equal(childFlag, false);
    assert.equal(parentFlag, false);
  });
}
