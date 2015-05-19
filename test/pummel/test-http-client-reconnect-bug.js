'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net'),
    util = require('util'),
    http = require('http');

var errorCount = 0;
var eofCount = 0;

var server = net.createServer(function(socket) {
  socket.end();
});

server.on('listening', function() {
  var client = http.createClient(common.PORT);

  client.on('error', function(err) {
    // We should receive one error
    console.log('ERROR! ' + err.message);
    errorCount++;
  });

  client.on('end', function() {
    // When we remove the old Client interface this will most likely have to be
    // changed.
    console.log('EOF!');
    eofCount++;
  });

  var request = client.request('GET', '/', {'host': 'localhost'});
  request.end();
  request.on('response', function(response) {
    console.log('STATUS: ' + response.statusCode);
  });
});

server.listen(common.PORT);

setTimeout(function() {
  server.close();
}, 500);


process.on('exit', function() {
  assert.equal(1, errorCount);
  assert.equal(1, eofCount);
});
