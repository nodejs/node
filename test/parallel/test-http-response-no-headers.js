'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const expected = {
  '0.9': 'I AM THE WALRUS',
  '1.0': 'I AM THE WALRUS',
  '1.1': ''
};

function test(httpVersion, callback) {
  const server = net.createServer(function(conn) {
    const reply = 'HTTP/' + httpVersion + ' 200 OK\r\n\r\n' +
                  expected[httpVersion];

    conn.end(reply);
  });

  server.listen(0, '127.0.0.1', common.mustCall(function() {
    const options = {
      host: '127.0.0.1',
      port: this.address().port
    };

    const req = http.get(options, common.mustCall(function(res) {
      let body = '';

      res.on('data', function(data) {
        body += data;
      });

      res.on('end', common.mustCall(function() {
        assert.strictEqual(body, expected[httpVersion]);
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
