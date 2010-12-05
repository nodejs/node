var common = require('../common');
var assert = require('assert');
var net = require('net');


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
var server1 = net.createServer(function(socket) { });

server1.listen(common.PORT);

setTimeout(function() {
  address1 = server1.address();
  console.log('address1 %j', address1);
  server1.close();
}, 100);


// Callback to listen()

var address2;
var server2 = net.createServer(function(socket) { });

server2.listen(common.PORT + 1, function() {
  address2 = server2.address();
  console.log('address2 %j', address2);
  server2.close();
});



process.on('exit', function() {
  assert.ok(address0.port > 100);
  assert.equal(common.PORT, address1.port);
  assert.equal(common.PORT + 1, address2.port);
});
