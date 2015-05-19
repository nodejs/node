'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var expected = {
  '0.9': 'I AM THE WALRUS',
  '1.0': 'I AM THE WALRUS',
  '1.1': ''
};

var gotExpected = false;

function test(httpVersion, callback) {
  process.on('exit', function() {
    assert(gotExpected);
  });

  var server = net.createServer(function(conn) {
    var reply = 'HTTP/' + httpVersion + ' 200 OK\r\n\r\n' +
                expected[httpVersion];

    conn.end(reply);
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    var options = {
      host: '127.0.0.1',
      port: common.PORT
    };

    var req = http.get(options, function(res) {
      var body = '';

      res.on('data', function(data) {
        body += data;
      });

      res.on('end', function() {
        assert.equal(body, expected[httpVersion]);
        gotExpected = true;
        server.close();
        if (callback) process.nextTick(callback);
      });
    });

    req.on('error', function(err) {
      throw err;
    });
  });
}

test('0.9', function() {
  test('1.0', function() {
    test('1.1');
  });
});
