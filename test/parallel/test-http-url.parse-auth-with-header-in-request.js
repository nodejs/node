'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var testURL = url.parse('http://asdf:qwer@localhost:' + common.PORT);
// the test here is if you set a specific authorization header in the
// request we should not override that with basic auth
testURL.headers = {
  Authorization: 'NoAuthForYOU'
};

function check(request) {
  // the correct authorization header is be passed
  assert.strictEqual(request.headers.authorization, 'NoAuthForYOU');
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
