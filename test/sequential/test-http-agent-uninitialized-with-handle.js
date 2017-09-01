'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');

const agent = new http.Agent({
  keepAlive: true,
});
const socket = new net.Socket();
// If _handle exists then internals assume a couple methods exist.
socket._handle = {
  ref() { },
  readStart() { },
};
const req = new http.ClientRequest(`http://localhost:${common.PORT}/`);

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
})).listen(common.PORT, common.mustCall(() => {
  // Manually add the socket without a _handle.
  agent.freeSockets[agent.getName(req)] = [socket];
  // Now force the agent to use the socket and check that _handle exists before
  // calling asyncReset().
  agent.addRequest(req, {});
  req.on('response', common.mustCall(() => {
    server.close();
  }));
  req.end();
}));
