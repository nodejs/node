'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var expectedHeaders = {
  'DELETE': ['host', 'connection'],
  'GET': ['host', 'connection'],
  'HEAD': ['host', 'connection'],
  'OPTIONS': ['host', 'connection'],
  'POST': ['host', 'connection', 'content-length'],
  'PUT': ['host', 'connection', 'content-length']
};

var expectedMethods = Object.keys(expectedHeaders);

var requestCount = 0;

var server = http.createServer(function(req, res) {
  requestCount++;
  res.end();

  assert(expectedHeaders.hasOwnProperty(req.method),
         req.method + ' was an unexpected method');

  var requestHeaders = Object.keys(req.headers);
  requestHeaders.forEach(function(header) {
    assert.notStrictEqual(
      expectedHeaders[req.method].indexOf(header.toLowerCase()),
      -1,
      header + ' shoud not exist for method ' + req.method
    );
  });

  assert.strictEqual(
    requestHeaders.length,
    expectedHeaders[req.method].length,
    'some headers were missing for method: ' + req.method
  );

  if (expectedMethods.length === requestCount)
    server.close();
});

server.listen(0, function() {
  expectedMethods.forEach(function(method) {
    http.request({
      method: method,
      port: server.address().port
    }).end();
  });
});
