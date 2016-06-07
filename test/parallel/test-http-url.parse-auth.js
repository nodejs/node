'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// username = "user", password = "pass:"
const testUrl = `http://user:pass%3A@localhost:${common.PORT}`;

function check(request) {
  // the correct authorization header is be passed
  assert.strictEqual(request.headers.authorization, 'Basic dXNlcjpwYXNzOg==');
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
