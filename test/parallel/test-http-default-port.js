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

const fixtures = require('../common/fixtures');
const http = require('http');
const https = require('https');
const assert = require('assert');
const hostExpect = 'localhost';
const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

for (const { mod, createServer } of [
  { mod: http, createServer: http.createServer },
  { mod: https, createServer: https.createServer.bind(null, options) }
]) {
  const server = createServer(common.mustCall((req, res) => {
    assert.strictEqual(req.headers.host, hostExpect);
    assert.strictEqual(req.headers['x-port'], `${server.address().port}`);
    res.writeHead(200);
    res.end('ok');
    server.close();
  })).listen(0, common.mustCall(() => {
    mod.globalAgent.defaultPort = server.address().port;
    mod.get({
      host: 'localhost',
      rejectUnauthorized: false,
      headers: {
        'x-port': server.address().port
      }
    }, common.mustCall((res) => {
      res.resume();
    }));
  }));
}
