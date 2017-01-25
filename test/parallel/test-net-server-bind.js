'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');


// With only a callback, server should get a port assigned by the OS

let address0;
const server0 = net.createServer(function(socket) { });

server0.listen(function() {
  address0 = server0.address();
  console.log('address0 %j', address0);
  server0.close();
});


// No callback to listen(), assume we can bind in 100 ms

let address1;
let connectionKey1;
const server1 = net.createServer(function(socket) { });

server1.listen(common.PORT);

setTimeout(function() {
  address1 = server1.address();
  connectionKey1 = server1._connectionKey;
  console.log('address1 %j', address1);
  server1.close();
}, 100);


// Callback to listen()

let address2;
const server2 = net.createServer(function(socket) { });

server2.listen(common.PORT + 1, function() {
  address2 = server2.address();
  console.log('address2 %j', address2);
  server2.close();
});


// Backlog argument

let address3;
const server3 = net.createServer(function(socket) { });

server3.listen(common.PORT + 2, '0.0.0.0', 127, function() {
  address3 = server3.address();
  console.log('address3 %j', address3);
  server3.close();
});


// Backlog argument without host argument

let address4;
const server4 = net.createServer(function(socket) { });

server4.listen(common.PORT + 3, 127, function() {
  address4 = server4.address();
  console.log('address4 %j', address4);
  server4.close();
});


process.on('exit', function() {
  assert.ok(address0.port > 100);
  assert.equal(common.PORT, address1.port);

  let expectedConnectionKey1;

  if (address1.family === 'IPv6')
    expectedConnectionKey1 = '6::::' + address1.port;
  else
    expectedConnectionKey1 = '4:0.0.0.0:' + address1.port;

  assert.equal(connectionKey1, expectedConnectionKey1);
  assert.equal(common.PORT + 1, address2.port);
  assert.equal(common.PORT + 2, address3.port);
  assert.equal(common.PORT + 3, address4.port);
});
