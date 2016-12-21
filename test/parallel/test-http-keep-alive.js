'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  const body = 'hello world\n';

  res.writeHead(200, {'Content-Length': body.length});
  res.write(body);
  res.end();
}, 3));

const agent = new http.Agent({maxSockets: 1});
const headers = {'connection': 'keep-alive'};
let name;

server.listen(0, common.mustCall(function() {
  name = agent.getName({ port: this.address().port });
  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    assert.strictEqual(agent.sockets[name].length, 1);
    assert.strictEqual(agent.requests[name].length, 2);
    response.resume();
  }));

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    assert.strictEqual(agent.sockets[name].length, 1);
    assert.strictEqual(agent.requests[name].length, 1);
    response.resume();
  }));

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      assert.strictEqual(agent.sockets[name].length, 1);
      assert(!agent.requests.hasOwnProperty(name));
      server.close();
    }));
    response.resume();
  }));
}));

process.on('exit', () => {
  assert(!agent.sockets.hasOwnProperty(name));
  assert(!agent.requests.hasOwnProperty(name));
});
