'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const testUrl = `http://localhost:${common.PORT}/asdf`;

function check(request) {
  // a path should come over
  assert.strictEqual(request.url, '/asdf');
}

const server = http.createServer(function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(common.PORT, function() {
  // make the request
  http.request(testUrl).end();
});
