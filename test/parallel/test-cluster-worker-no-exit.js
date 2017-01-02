'use strict';
const common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

var destroyed;
var success;
var worker;
var server;

// workers do not exit on disconnect, they exit under normal node rules: when
// they have nothing keeping their loop alive, like an active connection
//
// test this by:
//
// 1 creating a server, so worker can make a connection to something
// 2 disconnecting worker
// 3 wait to confirm it did not exit
// 4 destroy connection
// 5 confirm it does exit
if (cluster.isMaster) {
  server = net.createServer(function(conn) {
    server.close();
    worker.disconnect();
    worker.once('disconnect', function() {
      setTimeout(function() {
        conn.destroy();
        destroyed = true;
      }, 1000);
    }).once('exit', function() {
      // worker should not exit while it has a connection
      assert(destroyed, 'worker exited before connection destroyed');
      success = true;
    });

  }).listen(common.PORT, function() {
    var port = this.address().port;

    worker = cluster.fork()
      .on('online', function() {
        this.send({port: port});
      });
  });
  process.on('exit', function() {
    assert(success);
  });
} else {
  process.on('message', function(msg) {
    // we shouldn't exit, not while a network connection exists
    net.connect(msg.port);
  });
}
