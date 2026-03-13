'use strict';

const common = require('../common');
const { AsyncLocalStorage } = require('async_hooks');
const dc = require('diagnostics_channel');
const assert = require('assert');
const http = require('http');

const als = new AsyncLocalStorage();
let context;

// Bind requests to an AsyncLocalStorage context
dc.subscribe('http.server.request.start', common.mustCall((message) => {
  als.enterWith(message);
  context = message;
}));

// When the request ends, verify the context has been maintained
// and that the messages contain the expected data
dc.subscribe('http.server.response.finish', common.mustCall((message) => {
  const data = {
    request,
    response,
    server,
    socket: request.socket
  };

  // Context is maintained
  compare(als.getStore(), context);

  compare(context, data);
  compare(message, data);
}));

let request;
let response;

const server = http.createServer(common.mustCall((req, res) => {
  request = req;
  response = res;

  setTimeout(() => {
    res.end('done');
  }, 1);
}));

server.listen(() => {
  const { port } = server.address();
  http.get(`http://localhost:${port}`, (res) => {
    res.resume();
    res.on('end', () => {
      server.close();
    });
  });
});

function compare(a, b) {
  assert.strictEqual(a.request, b.request);
  assert.strictEqual(a.response, b.response);
  assert.strictEqual(a.socket, b.socket);
  assert.strictEqual(a.server, b.server);
}
