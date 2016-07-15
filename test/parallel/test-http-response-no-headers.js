'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var expected = {
  '0.9': 'I AM THE WALRUS',
  '1.0': 'I AM THE WALRUS',
  '1.1': ''
};

function test(httpVersion, callback) {
  var server = net.createServer(function(conn) {
    var reply = 'HTTP/' + httpVersion + ' 200 OK\r\n\r\n' +
                expected[httpVersion];

    conn.end(reply);
  });

  server.listen(0, '127.0.0.1', common.mustCall(function() {
    var options = {
      host: '127.0.0.1',
      port: this.address().port
    };

    var req = http.get(options, common.mustCall(function(res) {
      var body = '';

      res.on('data', function(data) {
        body += data;
      });

      res.on('end', common.mustCall(function() {
        assert.equal(body, expected[httpVersion]);
        server.close();
        if (callback) process.nextTick(callback);
      }));
    }));

    req.on('error', function(err) {
      throw err;
    });
  }));
}

test('0.9', function() {
  test('1.0', function() {
    test('1.1');
  });
});
