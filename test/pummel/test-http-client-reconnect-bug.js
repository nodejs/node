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

  client.addListener('error', function(err) {
    console.log('ERROR! ' + err.message);
    errorCount++;
  });

  client.addListener('end', function() {
    console.log('EOF!');
    eofCount++;
  });

  var request = client.request('GET', '/', {'host': 'localhost'});
  request.end();
  request.addListener('response', function(response) {
    console.log('STATUS: ' + response.statusCode);
  });
});

server.listen(common.PORT);

setTimeout(function() {
  server.close();
}, 500);


process.addListener('exit', function() {
  assert.equal(1, errorCount);
  assert.equal(1, eofCount);
});
