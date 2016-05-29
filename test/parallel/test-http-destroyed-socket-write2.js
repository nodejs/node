'use strict';
var common = require('../common');
var assert = require('assert');

// Verify that ECONNRESET is raised when writing to a http request
// where the server has ended the socket.

var http = require('http');
var server = http.createServer(function(req, res) {
  setImmediate(function() {
    res.destroy();
  });
});

server.listen(0, function() {
  var req = http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  });

  function write() {
    req.write('hello', function() {
      setImmediate(write);
    });
  }

  req.on('error', common.mustCall(function(er) {
    switch (er.code) {
      // This is the expected case
      case 'ECONNRESET':
        break;

      // On Windows, this sometimes manifests as ECONNABORTED
      case 'ECONNABORTED':
        break;

      // This test is timing sensitive so an EPIPE is not out of the question.
      // It should be infrequent, given the 50 ms timeout, but not impossible.
      case 'EPIPE':
        break;

      default:
        assert.strictEqual(er.code,
                           'ECONNRESET',
                           'Write to a torn down client should RESET or ABORT');
        break;
    }

    assert.equal(req.output.length, 0);
    assert.equal(req.outputEncodings.length, 0);
    server.close();
  }));

  req.on('response', function(res) {
    res.on('data', function(chunk) {
      common.fail('Should not receive response data');
    });
    res.on('end', function() {
      common.fail('Should not receive response end');
    });
  });

  write();
});
