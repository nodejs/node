'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');


var serverSocket = null;
var server = http.createServer(function(req, res) {
  // They should all come in on the same server socket.
  if (serverSocket) {
    assert.equal(req.socket, serverSocket);
  } else {
    serverSocket = req.socket;
  }

  res.end(req.url);
});
server.listen(common.PORT);

var agent = http.Agent({ keepAlive: true });


var clientSocket = null;
var expectRequests = 10;
var actualRequests = 0;


makeRequest(expectRequests);
function makeRequest(n) {
  if (n === 0) {
    server.close();
    agent.destroy();
    return;
  }

  var req = http.request({
    port: common.PORT,
    agent: agent,
    path: '/' + n
  });

  req.end();

  req.on('socket', function(sock) {
    if (clientSocket) {
      assert.equal(sock, clientSocket);
    } else {
      clientSocket = sock;
    }
  });

  req.on('response', function(res) {
    var data = '';
    res.setEncoding('utf8');
    res.on('data', function(c) {
      data += c;
    });
    res.on('end', function() {
      assert.equal(data, '/' + n);
      setTimeout(function() {
        actualRequests++;
        makeRequest(n - 1);
      }, 1);
    });
  });
}

process.on('exit', function() {
  assert.equal(actualRequests, expectRequests);
  console.log('ok');
});
