'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Connection': 'close'
  });
  res.write('hello ');
  res.write('world\n');
  res.end();
});

common.refreshTmpDir();

server.listen(common.PIPE, common.mustCall(function() {

  var options = {
    socketPath: common.PIPE,
    path: '/'
  };

  var req = http.get(options, common.mustCall(function(res) {
    assert.strictEqual(res.statusCode, 200);
    assert.strictEqual(res.headers['content-type'], 'text/plain');

    res.body = '';
    res.setEncoding('utf8');

    res.on('data', function(chunk) {
      res.body += chunk;
    });

    res.on('end', common.mustCall(function() {
      assert.strictEqual(res.body, 'hello world\n');
      server.close(common.mustCall(function(error) {
        assert.strictEqual(error, undefined);
        server.close(common.mustCall(function(error) {
          assert.strictEqual(error && error.message, 'Not running');
        }));
      }));
    }));
  }));

  req.on('error', function(e) {
    common.fail(e.stack);
  });

  req.end();

}));
