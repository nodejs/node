'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

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

server.listen(0, function() {
  const testURL = url.parse(`http://localhost:${this.address().port}/asdf`);

  // make the request
  http.request(testURL).end();
});
