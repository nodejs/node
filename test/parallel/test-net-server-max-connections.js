'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net');

// This test creates 200 connections to a server and sets the server's
// maxConnections property to 100. The first 100 connections make it through
// and the last 100 connections are rejected.

var N = 200;
var count = 0;
var closes = 0;
var waits = [];

var server = net.createServer(function(connection) {
  console.error('connect %d', count++);
  connection.write('hello');
  waits.push(function() { connection.end(); });
});

server.listen(0, function() {
  makeConnection(0);
});

server.maxConnections = N / 2;

console.error('server.maxConnections = %d', server.maxConnections);


function makeConnection(index) {
  var c = net.createConnection(server.address().port);
  var gotData = false;

  c.on('connect', function() {
    if (index + 1 < N) {
      makeConnection(index + 1);
    }

    c.on('close', function() {
      console.error('closed %d', index);
      closes++;

      if (closes < N / 2) {
        assert.ok(server.maxConnections <= index,
                  index +
                  ' was one of the first closed connections ' +
                  'but shouldnt have been');
      }

      if (closes === N / 2) {
        var cb;
        console.error('calling wait callback.');
        while (cb = waits.shift()) {
          cb();
        }
        server.close();
      }

      if (index < server.maxConnections) {
        assert.equal(true, gotData,
                     index + ' didn\'t get data, but should have');
      } else {
        assert.equal(false, gotData,
                     index + ' got data, but shouldn\'t have');
      }
    });
  });

  c.on('end', function() { c.end(); });

  c.on('data', function(b) {
    gotData = true;
    assert.ok(0 < b.length);
  });

  c.on('error', function(e) {
    // Retry if SmartOS and ECONNREFUSED. See
    // https://github.com/nodejs/node/issues/2663.
    if (common.isSunOS && (e.code === 'ECONNREFUSED')) {
      c.connect(server.address().port);
    }
    console.error('error %d: %s', index, e);
  });
}


process.on('exit', function() {
  assert.equal(N, closes);
});
