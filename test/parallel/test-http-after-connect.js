'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

var clientResponses = 0;

const server = http.createServer(common.mustCall(function(req, res) {
  console.error('Server got GET request');
  req.resume();
  res.writeHead(200);
  res.write('');
  setTimeout(function() {
    res.end(req.url);
  }, 50);
}, 2));
server.on('connect', common.mustCall(function(req, socket) {
  console.error('Server got CONNECT request');
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.resume();
  socket.on('end', function() {
    socket.end();
  });
}));
server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', common.mustCall(function(res, socket) {
    console.error('Client got CONNECT response');
    socket.end();
    socket.on('end', function() {
      doRequest(0);
      doRequest(1);
    });
    socket.resume();
  }));
  req.end();
});

function doRequest(i) {
  http.get({
    port: server.address().port,
    path: '/request' + i
  }, common.mustCall(function(res) {
    console.error('Client got GET response');
    var data = '';
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      data += chunk;
    });
    res.on('end', function() {
      assert.strictEqual(data, '/request' + i);
      ++clientResponses;
      if (clientResponses === 2) {
        server.close();
      }
    });
  }));
}
