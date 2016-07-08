'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};


var server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});


var responses = 0;
var N = 4;
var M = 4;

server.listen(0, function() {
  for (var i = 0; i < N; i++) {
    setTimeout(function() {
      for (var j = 0; j < M; j++) {
        https.get({
          path: '/',
          port: server.address().port,
          rejectUnauthorized: false
        }, function(res) {
          res.resume();
          console.log(res.statusCode);
          if (++responses == N * M) server.close();
        }).on('error', function(e) {
          console.log(e.message);
          process.exit(1);
        });
      }
    }, i);
  }
});


process.on('exit', function() {
  assert.equal(N * M, responses);
});
