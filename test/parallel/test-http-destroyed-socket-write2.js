var common = require('../common');
var assert = require('assert');

// Verify that ECONNRESET is raised when writing to a http request
// where the server has ended the socket.

var http = require('http');
var net = require('net');
var server = http.createServer(function(req, res) {
  setImmediate(function() {
    res.destroy();
  });
});

server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    path: '/',
    method: 'POST'
  });

  var timer = setTimeout(write, 50);
  var writes = 0;

  function write() {
    if (++writes === 128) {
      clearTimeout(timer);
      req.end();
      test();
    } else {
      req.write('hello', function() {
        timer = setImmediate(write);
      });
    }
  }

  var gotError = false;
  var sawData = false;
  var sawEnd = false;

  req.on('error', function(er) {
    assert(!gotError);
    gotError = true;
    switch (er.code) {
      // This is the expected case
      case 'ECONNRESET':
      // On windows this sometimes manifests as ECONNABORTED
      case 'ECONNABORTED':
      // This test is timing sensitive so an EPIPE is not out of the question.
      // It should be infrequent, given the 50 ms timeout, but not impossible.
      case 'EPIPE':
        break;
      default:
        assert.strictEqual(er.code,
          'ECONNRESET',
          'Writing to a torn down client should RESET or ABORT');
        break;
    }
    clearTimeout(timer);
    console.log('ECONNRESET was raised after %d writes', writes);
    test();
  });

  req.on('response', function(res) {
    res.on('data', function(chunk) {
      console.error('saw data: ' + chunk);
      sawData = true;
    });
    res.on('end', function() {
      console.error('saw end');
      sawEnd = true;
    });
  });

  var closed = false;

  function test() {
    if (closed)
      return;

    server.close();
    closed = true;

    if (req.output.length || req.outputEncodings.length)
      console.error('bad happened', req.output, req.outputEncodings);

    assert.equal(req.output.length, 0);
    assert.equal(req.outputEncodings, 0);
    assert(gotError);
    assert(!sawData);
    assert(!sawEnd);
    console.log('ok');
  }
});
