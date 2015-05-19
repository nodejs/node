'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var expectedHeadersMultipleWrites = {
  'connection': 'close',
  'transfer-encoding': 'chunked',
};

var expectedHeadersEndWithData = {
  'connection': 'close',
  'content-length': 'hello world'.length,
};

var expectedHeadersEndNoData = {
  'connection': 'close',
  'content-length': '0',
};

var receivedRequests = 0;
var totalRequests = 3;

var server = http.createServer(function(req, res) {
  res.removeHeader('Date');

  switch (req.url.substr(1)) {
    case 'multiple-writes':
      assert.deepEqual(req.headers, expectedHeadersMultipleWrites);
      res.write('hello');
      res.end('world');
      break;
    case 'end-with-data':
      assert.deepEqual(req.headers, expectedHeadersEndWithData);
      res.end('hello world');
      break;
    case 'empty':
      assert.deepEqual(req.headers, expectedHeadersEndNoData);
      res.end();
      break;
    default:
      throw new Error('Unreachable');
      break;
  }

  receivedRequests++;
  if (totalRequests === receivedRequests) server.close();
});

server.listen(common.PORT, function() {
  var req;

  req = http.request({
    port: common.PORT,
    method: 'POST',
    path: '/multiple-writes'
  });
  req.removeHeader('Date');
  req.removeHeader('Host');
  req.write('hello ');
  req.end('world');
  req.on('response', function(res) {
    assert.deepEqual(res.headers, expectedHeadersMultipleWrites);
  });

  req = http.request({
    port: common.PORT,
    method: 'POST',
    path: '/end-with-data'
  });
  req.removeHeader('Date');
  req.removeHeader('Host');
  req.end('hello world');
  req.on('response', function(res) {
    assert.deepEqual(res.headers, expectedHeadersEndWithData);
  });

  req = http.request({
    port: common.PORT,
    method: 'POST',
    path: '/empty'
  });
  req.removeHeader('Date');
  req.removeHeader('Host');
  req.end();
  req.on('response', function(res) {
    assert.deepEqual(res.headers, expectedHeadersEndNoData);
  });

});
