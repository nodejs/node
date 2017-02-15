'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const url = require('url');

let testURL;

// make sure the basics work
function check(request) {
  // default method should still be get
  assert.strictEqual(request.method, 'GET');
  // there are no URL params, so you should not see any
  assert.strictEqual(request.url, '/');
  // the host header should use the url.parse.hostname
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
  testURL = url.parse(`http://localhost:${this.address().port}`);

  // make the request
  const clientRequest = http.request(testURL);
  // since there is a little magic with the agent
  // make sure that an http request uses the http.Agent
  assert.ok(clientRequest.agent instanceof http.Agent);
  clientRequest.end();
});
