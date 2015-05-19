'use strict';
// test that errors propagated from cluster children are properly
// received in their master creates an EADDRINUSE condition by also
// forking a child process to listen on a socket

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;
var fs = require('fs');
var net = require('net');


if (cluster.isMaster) {
  var worker = cluster.fork();
  var gotError = 0;
  worker.on('message', function(err) {
    gotError++;
    console.log(err);
    assert.strictEqual('EADDRINUSE', err.code);
    worker.disconnect();
  });
  process.on('exit', function() {
    console.log('master exited');
    try {
      fs.unlinkSync(common.PIPE);
    } catch (e) {
    }
    assert.equal(gotError, 1);
  });
} else {
  var cp = fork(common.fixturesDir + '/listen-on-socket-and-exit.js',
                { stdio: 'inherit' });

  // message from the child indicates it's ready and listening
  cp.on('message', function() {
    var server = net.createServer().listen(common.PIPE, function() {
      console.log('parent listening, should not be!');
    });

    server.on('error', function(err) {
      console.log('parent error, ending');
      // message to child process tells it to exit
      cp.send('end');
      // propagate error to parent
      process.send(err);
    });
  });
}
