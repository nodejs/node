'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.Server(common.mustCall((req, res) => {
  res.writeHead(200);
  res.end('Hello, World!');
}));

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent();
  const name = agent.getName({ port: server.address().port });
  http.globalAgent = agent;

  makeRequest();
  assert(agent.sockets.hasOwnProperty(name)); // Agent has indeed been used
}));

function makeRequest() {
  const req = http.get({
    port: server.address().port
  });
  req.on('close', () =>
    server.close());
}
