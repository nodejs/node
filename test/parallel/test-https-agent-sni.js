'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const https = require('https');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const TOTAL = 4;
var waiting = TOTAL;

const server = https.Server(options, function(req, res) {
  if (--waiting === 0) server.close();

  res.writeHead(200, {
    'x-sni': req.socket.servername
  });
  res.end('hello world');
});

server.listen(common.PORT, function() {
  function expectResponse(id) {
    return common.mustCall(function(res) {
      res.resume();
      assert.equal(res.headers['x-sni'], 'sni.' + id);
    });
  }

  var agent = new https.Agent({
    maxSockets: 1
  });
  for (var j = 0; j < TOTAL; j++) {
    https.get({
      agent: agent,

      path: '/',
      port: common.PORT,
      host: '127.0.0.1',
      servername: 'sni.' + j,
      rejectUnauthorized: false
    }, expectResponse(j));
  }
});
