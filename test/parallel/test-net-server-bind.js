'use strict';
require('../common');
var assert = require('assert');
var net = require('net');


function assertValidPort(port) {
  assert.strictEqual(typeof port, 'number');
  assert.ok(isFinite(port));
  assert.ok(port > 0);
}

// With only a callback, server should get a port assigned by the OS

var address0;
var server0 = net.createServer(function(socket) { });

server0.listen(function() {
  address0 = server0.address();
  console.log('address0 %j', address0);
  server0.close();
});


// No callback to listen(), assume we can bind in 100 ms

var address1;
var connectionKey1;
var server1 = net.createServer(function(socket) { });

server1.listen(0);

setTimeout(function() {
  address1 = server1.address();
  connectionKey1 = server1._connectionKey;
  console.log('address1 %j', address1);
  server1.close();
}, 100);


// Callback to listen()

var address2;
var server2 = net.createServer(function(socket) { });

server2.listen(0, function() {
  address2 = server2.address();
  console.log('address2 %j', address2);
  server2.close();
});


// Backlog argument

var address3;
var server3 = net.createServer(function(socket) { });

server3.listen(0, '0.0.0.0', 127, function() {
  address3 = server3.address();
  console.log('address3 %j', address3);
  server3.close();
});


// Backlog argument without host argument

var address4;
var server4 = net.createServer(function(socket) { });

server4.listen(0, 127, function() {
  address4 = server4.address();
  console.log('address4 %j', address4);
  server4.close();
});


process.on('exit', function() {
  assert.ok(address0.port > 100);

  assertValidPort(address1.port);
  var expectedConnectionKey1;
  if (address1.family === 'IPv6')
    expectedConnectionKey1 = '6::::' + address1.port;
  else
    expectedConnectionKey1 = '4:0.0.0.0:' + address1.port;
  assert.equal(connectionKey1, expectedConnectionKey1);

  assertValidPort(address2.port);
  assertValidPort(address3.port);
  assertValidPort(address4.port);
});
