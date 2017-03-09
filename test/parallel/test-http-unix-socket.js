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

  const options = {
    socketPath: common.PIPE,
    path: '/'
  };

  const req = http.get(options, common.mustCall(function(res) {
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
