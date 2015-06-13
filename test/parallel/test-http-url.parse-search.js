'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var testURL = url.parse('http://localhost:' + common.PORT + '/asdf?qwer=zxcv');

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

server.listen(common.PORT, function() {
  // make the request
  http.request(testURL).end();
});
