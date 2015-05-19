'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

// username = "user", password = "pass:"
var testURL = url.parse('http://user:pass%3A@localhost:' + common.PORT);

function check(request) {
  // the correct authorization header is be passed
  assert.strictEqual(request.headers.authorization, 'Basic dXNlcjpwYXNzOg==');
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
