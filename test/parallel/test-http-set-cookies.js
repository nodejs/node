'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var nresponses = 0;

var server = http.createServer(function(req, res) {
  if (req.url == '/one') {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['content-type', 'text/plain']]);
    res.end('one\n');
  } else {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['set-cookie', 'B'],
                        ['content-type', 'text/plain']]);
    res.end('two\n');
  }
});
server.listen(common.PORT);

server.on('listening', function() {
  //
  // one set-cookie header
  //
  http.get({ port: common.PORT, path: '/one' }, function(res) {
    // set-cookie headers are always return in an array.
    // even if there is only one.
    assert.deepEqual(['A'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      if (++nresponses == 2) {
        server.close();
      }
    });
  });

  // two set-cookie headers

  http.get({ port: common.PORT, path: '/two' }, function(res) {
    assert.deepEqual(['A', 'B'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      if (++nresponses == 2) {
        server.close();
      }
    });
  });

});

process.on('exit', function() {
  assert.equal(2, nresponses);
});
