'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const N = 100;
let responses = 0;

const server = http.createServer(function(req, res) {
  res.end('Hello');
});

server.listen(0, function() {
  http.globalAgent.maxSockets = 1;
  let parser;
  for (let i = 0; i < N; ++i) {
    (function makeRequest(i) {
      const req = http.get({port: server.address().port}, function(res) {
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
  assert.strictEqual(responses, N);
});
