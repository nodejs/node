'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

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

server.listen(0, function() {
  const port = this.address().port;
  // username = "user", password = "pass:"
  const testURL = url.parse(`http://user:pass%3A@localhost:${port}`);

  // make the request
  http.request(testURL).end();
});
