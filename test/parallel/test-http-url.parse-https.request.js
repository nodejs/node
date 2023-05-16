// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { readKey } = require('../common/fixtures');

const assert = require('assert');
const https = require('https');
const url = require('url');

// https options
const httpsOptions = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem')
};

function check(request) {
  // Assert that I'm https
  assert.ok(request.socket._secureEstablished);
}

const server = https.createServer(httpsOptions, function(request, response) {
  // Run the check function
  check(request);
  response.writeHead(200, {});
  response.end('ok');
  server.close();
});

server.listen(0, function() {
  const testURL = url.parse(`https://localhost:${this.address().port}`);
  testURL.rejectUnauthorized = false;

  // make the request
  const clientRequest = https.request(testURL);
  // Since there is a little magic with the agent
  // make sure that the request uses the https.Agent
  assert.ok(clientRequest.agent instanceof https.Agent);
  clientRequest.end();
});
