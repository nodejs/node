'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var N = 100;
var responses = 0;

var server = http.createServer(function(req, res) {
  res.end('Hello');
});

server.listen(0, function() {
  http.globalAgent.maxSockets = 1;
  var parser;
  for (var i = 0; i < N; ++i) {
    (function makeRequest(i) {
      var req = http.get({port: server.address().port}, function(res) {
        if (!parser) {
          parser = req.parser;
        } else {
          assert.strictEqual(req.parser, parser);
        }

        if (++responses === N) {
          server.close();
        }
        res.resume();
      });
    })(i);
  }
});

process.on('exit', function() {
  assert.equal(responses, N);
});
