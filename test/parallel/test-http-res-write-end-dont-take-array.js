'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var test = 1;

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  if (test === 1) {
    // write should accept string
    res.write('string');
    // write should accept buffer
    res.write(Buffer.from('asdf'));

    // write should not accept an Array
    assert.throws(function() {
      res.write(['array']);
    }, TypeError, 'first argument must be a string or Buffer');

    // end should not accept an Array
    assert.throws(function() {
      res.end(['moo']);
    }, TypeError, 'first argument must be a string or Buffer');

    // end should accept string
    res.end('string');
  } else if (test === 2) {
    // end should accept Buffer
    res.end(Buffer.from('asdf'));
  }
});

server.listen(0, function() {
  // just make a request, other tests handle responses
  http.get({port: this.address().port}, function(res) {
    res.resume();
    // lazy serial test, because we can only call end once per request
    test += 1;
    // do it again to test .end(Buffer);
    http.get({port: server.address().port}, function(res) {
      res.resume();
      server.close();
    });
  });
});
