'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

function check(request) {
  // a path should come over with params
  assert.strictEqual(request.url, '/asdf?qwer=zxcv');
}

var server = http.createServer(function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(0, function() {
  const port = this.address().port;
  var testURL = url.parse(`http://localhost:${port}/asdf?qwer=zxcv`);

  // make the request
  http.request(testURL).end();
});
