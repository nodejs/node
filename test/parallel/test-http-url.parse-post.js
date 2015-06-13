'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var testURL = url.parse('http://localhost:' + common.PORT + '/asdf?qwer=zxcv');
testURL.method = 'POST';

function check(request) {
  //url.parse should not mess with the method
  assert.strictEqual(request.method, 'POST');
  //everything else should be right
  assert.strictEqual(request.url, '/asdf?qwer=zxcv');
  //the host header should use the url.parse.hostname
  assert.strictEqual(request.headers.host,
      testURL.hostname + ':' + testURL.port);
}

var server = http.createServer(function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(common.PORT, function() {
  // make the request
  http.request(testURL).end();
});
