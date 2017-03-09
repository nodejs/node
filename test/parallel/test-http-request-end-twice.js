'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.Server(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('hello world\n');
});
server.listen(0, function() {
  const req = http.get({port: this.address().port}, function(res) {
    res.on('end', function() {
      assert.ok(!req.end());
      server.close();
    });
    res.resume();
  });
});
