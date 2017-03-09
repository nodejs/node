'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const body = 'hello world\n';
const headers = {'connection': 'keep-alive'};

const server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': body.length, 'Connection': 'close'});
  res.write(body);
  res.end();
});

let connectCount = 0;


server.listen(0, function() {
  const agent = new http.Agent({ maxSockets: 1 });
  const name = agent.getName({ port: this.address().port });
  let request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(res) {
    assert.equal(1, agent.sockets[name].length);
    res.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();

  request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(res) {
    assert.equal(1, agent.sockets[name].length);
    res.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();
  request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(response) {
    response.on('end', function() {
      assert.equal(1, agent.sockets[name].length);
      server.close();
    });
    response.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();
});

process.on('exit', function() {
  assert.equal(3, connectCount);
});
