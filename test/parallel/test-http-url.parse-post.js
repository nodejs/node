'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

let testURL;

function check(request) {
  //url.parse should not mess with the method
  assert.strictEqual(request.method, 'POST');
  //everything else should be right
  assert.strictEqual(request.url, '/asdf?qwer=zxcv');
  //the host header should use the url.parse.hostname
  assert.strictEqual(request.headers.host,
                     testURL.hostname + ':' + testURL.port);
}

const server = http.createServer(function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(0, function() {
  testURL = url.parse(`http://localhost:${this.address().port}/asdf?qwer=zxcv`);
  testURL.method = 'POST';

  // make the request
  http.request(testURL).end();
});
