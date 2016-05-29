'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var body = 'hello world\n';

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': body.length});
  res.write(body);
  res.end();
});

var agent = new http.Agent({maxSockets: 1});
var headers = {'connection': 'keep-alive'};
var name;

server.listen(0, function() {
  name = agent.getName({ port: this.address().port });
  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, function(response) {
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.requests[name].length, 2);
    response.resume();
  });

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, function(response) {
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.requests[name].length, 1);
    response.resume();
  });

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, function(response) {
    response.on('end', function() {
      assert.equal(agent.sockets[name].length, 1);
      assert(!agent.requests.hasOwnProperty(name));
      server.close();
    });
    response.resume();
  });
});

process.on('exit', function() {
  assert(!agent.sockets.hasOwnProperty(name));
  assert(!agent.requests.hasOwnProperty(name));
});
