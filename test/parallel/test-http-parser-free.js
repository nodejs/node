'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var N = 100;
var responses = 0;

var server = http.createServer(function(req, res) {
  res.end('Hello');
});

server.listen(common.PORT, function() {
  http.globalAgent.maxSockets = 1;
  var parser;
  for (var i = 0; i < N; ++i) {
    (function makeRequest(i) {
      var req = http.get({port: common.PORT}, function(res) {
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
