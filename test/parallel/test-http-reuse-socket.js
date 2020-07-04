'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const Countdown = require('../common/countdown');

// The HEAD:204, GET:200 is the most pathological test case.
// GETs following a 204 response with a content-encoding header failed.
// Responses without bodies and without content-length or encoding caused
// the socket to be closed.
const codes = [204, 200, 200, 304, 200];
const methods = ['HEAD', 'HEAD', 'GET', 'HEAD', 'GET'];

const sockets = [];
const agent = new http.Agent();
agent.maxSockets = 1;

const countdown = new Countdown(codes.length, () => server.close());

const server = http.createServer(common.mustCall((req, res) => {
  const code = codes.shift();
  assert.strictEqual(typeof code, 'number');
  assert.ok(code > 0);
  res.writeHead(code, {});
  res.end();
}, codes.length));

function nextRequest() {
  const request = http.request({
    port: server.address().port,
    path: '/',
    agent: agent,
    method: methods.shift()
  }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      if (countdown.dec()) {
        nextRequest();
      }
      assert.strictEqual(sockets.length, 1);
    }));
    response.resume();
  }));
  request.on('socket', common.mustCall((socket) => {
    if (!sockets.includes(socket)) {
      sockets.push(socket);
    }
  }));
  request.end();
}

server.listen(0, common.mustCall(nextRequest));
