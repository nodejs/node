'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var testServer = new http.Server(function(req, res) {
  res.writeHead(200);
  res.end('Hello world');
});

testServer.listen(0, function() {
  http.get({ port: this.address().port }, function(res) {
    assert.equal(res.readable, true, 'res.readable initially true');
    res.on('end', function() {
      assert.equal(res.readable, false, 'res.readable set to false after end');
      testServer.close();
    });
    res.resume();
  });
});
