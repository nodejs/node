'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var url = require('url');
var fs = require('fs');

// https options
var httpsOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function check(request) {
  // assert that I'm https
  assert.ok(request.socket._secureEstablished);
}

var server = https.createServer(httpsOptions, function(request, response) {
  // run the check function
  check.call(this, request, response);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(0, function() {
  var testURL = url.parse(`https://localhost:${this.address().port}`);
  testURL.rejectUnauthorized = false;

  // make the request
  var clientRequest = https.request(testURL);
  // since there is a little magic with the agent
  // make sure that the request uses the https.Agent
  assert.ok(clientRequest.agent instanceof https.Agent);
  clientRequest.end();
});
