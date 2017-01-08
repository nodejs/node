'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

function check(request) {
  // a path should come over with params
  assert.strictEqual(request.url, '/asdf?qwer=zxcv');
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
  const testURL = url.parse(`http://localhost:${port}/asdf?qwer=zxcv`);

  // make the request
  http.request(testURL).end();
});
