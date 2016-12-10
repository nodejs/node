'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var nresponses = 0;

var server = http.createServer(function(req, res) {
  if (req.url === '/one') {
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
server.listen(0);

server.on('listening', function() {
  //
  // one set-cookie header
  //
  http.get({ port: this.address().port, path: '/one' }, function(res) {
    // set-cookie headers are always return in an array.
    // even if there is only one.
    assert.deepStrictEqual(['A'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      if (++nresponses === 2) {
        server.close();
      }
    });
  });

  // two set-cookie headers

  http.get({ port: this.address().port, path: '/two' }, function(res) {
    assert.deepStrictEqual(['A', 'B'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      if (++nresponses === 2) {
        server.close();
      }
    });
  });

});

process.on('exit', function() {
  assert.equal(2, nresponses);
});
