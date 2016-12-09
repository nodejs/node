'use strict';
require('../common');
var assert = require('assert');

var http = require('http');

var serverEndCb = false;
var serverIncoming = '';
var serverIncomingExpect = 'bazquuxblerg';

var clientEndCb = false;
var clientIncoming = '';
var clientIncomingExpect = 'asdffoobar';

process.on('exit', function() {
  assert(serverEndCb);
  assert.equal(serverIncoming, serverIncomingExpect);
  assert(clientEndCb);
  assert.equal(clientIncoming, clientIncomingExpect);
  console.log('ok');
});

// Verify that we get a callback when we do res.write(..., cb)
var server = http.createServer(function(req, res) {
  res.statusCode = 400;
  res.end('Bad Request.\nMust send Expect:100-continue\n');
});

server.on('checkContinue', function(req, res) {
  server.close();
  assert.equal(req.method, 'PUT');
  res.writeContinue(function() {
    // continue has been written
    req.on('end', function() {
      res.write('asdf', function(er) {
        assert.ifError(er);
        res.write('foo', 'ascii', function(er) {
          assert.ifError(er);
          res.end(Buffer.from('bar'), 'buffer', function(er) {
            serverEndCb = true;
          });
        });
      });
    });
  });

  req.setEncoding('ascii');
  req.on('data', function(c) {
    serverIncoming += c;
  });
});

server.listen(0, function() {
  var req = http.request({
    port: this.address().port,
    method: 'PUT',
    headers: { 'expect': '100-continue' }
  });
  req.on('continue', function() {
    // ok, good to go.
    req.write('YmF6', 'base64', function(er) {
      assert.ifError(er);
      req.write(Buffer.from('quux'), function(er) {
        assert.ifError(er);
        req.end('626c657267', 'hex', function(er) {
          assert.ifError(er);
          clientEndCb = true;
        });
      });
    });
  });
  req.on('response', function(res) {
    // this should not come until after the end is flushed out
    assert(clientEndCb);
    res.setEncoding('ascii');
    res.on('data', function(c) {
      clientIncoming += c;
    });
  });
});
