'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};


const server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});


var responses = 0;
const N = 4;
const M = 4;


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
          assert.strictEqual(res.statusCode, 200);
          if (++responses === N * M) server.close();
        }).on('error', function(e) {
          throw e;
        });
      }
    }, i);
  }
});


process.on('exit', function() {
  assert.strictEqual(N * M, responses);
});
