'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const url = require('url');
const fs = require('fs');

// https options
const httpsOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function check(request) {
  // assert that I'm https
  assert.ok(request.socket._secureEstablished);
}

const server = https.createServer(httpsOptions, function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(0, function() {
  const testURL = url.parse(`https://localhost:${this.address().port}`);
  testURL.rejectUnauthorized = false;

  // make the request
  const clientRequest = https.request(testURL);
  // since there is a little magic with the agent
  // make sure that the request uses the https.Agent
  assert.ok(clientRequest.agent instanceof https.Agent);
  clientRequest.end();
});
