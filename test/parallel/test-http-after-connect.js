'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var serverConnected = false;
var serverRequests = 0;
var clientResponses = 0;

var server = http.createServer(function(req, res) {
  console.error('Server got GET request');
  req.resume();
  ++serverRequests;
  res.writeHead(200);
  res.write('');
  setTimeout(function() {
    res.end(req.url);
  }, 50);
});
server.on('connect', function(req, socket, firstBodyChunk) {
  console.error('Server got CONNECT request');
  serverConnected = true;
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.resume();
  socket.on('end', function() {
    socket.end();
  });
});
server.listen(0, function() {
  var req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', function(res, socket, firstBodyChunk) {
    console.error('Client got CONNECT response');
    socket.end();
    socket.on('end', function() {
      doRequest(0);
      doRequest(1);
    });
    socket.resume();
  });
  req.end();
});

function doRequest(i) {
  http.get({
    port: server.address().port,
    path: '/request' + i
  }, function(res) {
    console.error('Client got GET response');
    var data = '';
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      data += chunk;
    });
    res.on('end', function() {
      assert.equal(data, '/request' + i);
      ++clientResponses;
      if (clientResponses === 2) {
        server.close();
      }
    });
  });
}

process.on('exit', function() {
  assert(serverConnected);
  assert.equal(serverRequests, 2);
  assert.equal(clientResponses, 2);
});
