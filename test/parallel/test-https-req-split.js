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
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');

// disable strict server certificate validation by the client
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const https = require('https');

const tls = require('tls');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

// Force splitting incoming data
tls.SLAB_BUFFER_SIZE = 1;

const server = https.createServer(options);
server.on('upgrade', common.mustCall(function(req, socket, upgrade) {
  socket.on('data', function(data) {
    throw new Error(`Unexpected data: ${data}`);
  });
  socket.end('HTTP/1.1 200 Ok\r\n\r\n');
}));

server.listen(0, function() {
  const req = https.request({
    host: '127.0.0.1',
    port: this.address().port,
    agent: false,
    headers: {
      Connection: 'Upgrade',
      Upgrade: 'Websocket'
    }
  }, function() {
    req.socket.destroy();
    server.close();
  });

  req.end();
});
