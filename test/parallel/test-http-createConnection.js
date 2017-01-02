'use strict';
const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
}, 4)).listen(0, '127.0.0.1', function() {
  let fn = common.mustCall(createConnection);
  http.get({ createConnection: fn }, function(res) {
    res.resume();
    fn = common.mustCall(createConnectionAsync);
    http.get({ createConnection: fn }, function(res) {
      res.resume();
      fn = common.mustCall(createConnectionBoth1);
      http.get({ createConnection: fn }, function(res) {
        res.resume();
        fn = common.mustCall(createConnectionBoth2);
        http.get({ createConnection: fn }, function(res) {
          res.resume();
          fn = common.mustCall(createConnectionError);
          http.get({ createConnection: fn }, function(res) {
            common.fail('Unexpected response callback');
          }).on('error', common.mustCall(function(err) {
            assert.equal(err.message, 'Could not create socket');
            server.close();
          }));
        });
      });
    });
  });
});

function createConnection() {
  return net.createConnection(server.address().port, '127.0.0.1');
}

function createConnectionAsync(options, cb) {
  setImmediate(function() {
    cb(null, net.createConnection(server.address().port, '127.0.0.1'));
  });
}

function createConnectionBoth1(options, cb) {
  const socket = net.createConnection(server.address().port, '127.0.0.1');
  setImmediate(function() {
    cb(null, socket);
  });
  return socket;
}

function createConnectionBoth2(options, cb) {
  const socket = net.createConnection(server.address().port, '127.0.0.1');
  cb(null, socket);
  return socket;
}

function createConnectionError(options, cb) {
  process.nextTick(cb, new Error('Could not create socket'));
}
