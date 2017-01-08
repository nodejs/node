'use strict';
require('../common');
const assert = require('assert');

const http = require('http');


let serverSocket = null;
const server = http.createServer(function(req, res) {
  // They should all come in on the same server socket.
  if (serverSocket) {
    assert.strictEqual(req.socket, serverSocket);
  } else {
    serverSocket = req.socket;
  }

  res.end(req.url);
});
server.listen(0, function() {
  makeRequest(expectRequests);
});

const agent = http.Agent({ keepAlive: true });


let clientSocket = null;
let expectRequests = 10;
let actualRequests = 0;


function makeRequest(n) {
  if (n === 0) {
    server.close();
    agent.destroy();
    return;
  }

  const req = http.request({
    port: server.address().port,
    agent: agent,
    path: '/' + n
  });

  req.end();

  req.on('socket', function(sock) {
    if (clientSocket) {
      assert.strictEqual(sock, clientSocket);
    } else {
      clientSocket = sock;
    }
  });

  req.on('response', function(res) {
    let data = '';
    res.setEncoding('utf8');
    res.on('data', function(c) {
      data += c;
    });
    res.on('end', function() {
      assert.strictEqual(data, '/' + n);
      setTimeout(function() {
        actualRequests++;
        makeRequest(n - 1);
      }, 1);
    });
  });
}

process.on('exit', function() {
  assert.strictEqual(actualRequests, expectRequests);
  console.log('ok');
});
