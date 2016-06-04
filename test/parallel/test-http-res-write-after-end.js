'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var responseError;

var server = http.Server(function(req, res) {
  res.on('error', function onResError(err) {
    responseError = err;
  });

  res.write('This should write.');
  res.end();

  var r = res.write('This should raise an error.');
  assert.equal(r, false, 'write after end should return false');
});

server.listen(common.PORT, function() {
  http.get({port: common.PORT}, function(res) {
    server.close();
  });
});

process.on('exit', function onProcessExit(code) {
  assert(responseError, 'response should have emitted error');
  assert.equal(responseError.message, 'write after end');
});
