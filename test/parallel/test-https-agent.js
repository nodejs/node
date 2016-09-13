'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const fs = require('fs');

//Gets SSL Certificate and SSL Key.
const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

//Starts up a https server using the SSL Certificate and Key.
const server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});


var responses = 0;
const N = 4;
const M = 4;

/*
the part being tested, Loops around 4 times on N,
Each time it loops around another 4 times on M,
each time sending a https GET request for route '/'
each inner loop records the number of responses
and closes the https server once the https request has been send 16 times.
 */

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
          if (++responses === N * M) server.close();
        }).on('error', function(e) {
          console.log(e.message);
          process.exit(1);
        });
      }
    }, i);
  }
});

/*
Asserts the number of responses match the number of loops,
making sure all responses were sent successfully.
*/

process.on('exit', function() {
  assert.strictEqual(N * M, responses);
});
