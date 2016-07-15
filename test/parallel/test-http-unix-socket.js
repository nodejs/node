'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
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
    assert.equal(res.statusCode, 200);
    assert.equal(res.headers['content-type'], 'text/plain');

    res.body = '';
    res.setEncoding('utf8');

    res.on('data', function(chunk) {
      res.body += chunk;
    });

    res.on('end', common.mustCall(function() {
      assert.equal(res.body, 'hello world\n');
      server.close(function(error) {
        assert.equal(error, undefined);
        server.close(function(error) {
          assert.equal(error && error.message, 'Not running');
        });
      });
    }));
  }));

  req.on('error', function(e) {
    console.log(e.stack);
    process.exit(1);
  });

  req.end();

}));
