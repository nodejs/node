'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert(res.writable, 'Res should be writable when it is received \
                          and opened.');
  assert(!res.finished, 'Res shouldn\'t be finished when it is received \
                          and opened.');
  res.end();
  assert(!res.writable, 'Res shouldn\'t be writable after it was closed.');
  assert(res.finished, 'Res should be finished after it was closed.');

  server.close();
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  const clientRequest = http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  });

  assert(clientRequest.writable, 'ClientRequest should be writable when \
                            it is created.');
  clientRequest.end();
  assert(!clientRequest.writable, 'ClientRequest shouldn\'t be writable \
                            after it was closed.');
}));
