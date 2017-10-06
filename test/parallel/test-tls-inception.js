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

const assert = require('assert');
const tls = require('tls');

const fs = require('fs');
const path = require('path');
const net = require('net');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const body = 'A'.repeat(40000);

// the "proxy" server
const a = tls.createServer(options, function(socket) {
  const myOptions = {
    host: '127.0.0.1',
    port: b.address().port,
    rejectUnauthorized: false
  };
  const dest = net.connect(myOptions);
  dest.pipe(socket);
  socket.pipe(dest);

  dest.on('end', function() {
    socket.destroy();
  });
});

// the "target" server
const b = tls.createServer(options, function(socket) {
  socket.end(body);
});

a.listen(0, function() {
  b.listen(0, function() {
    const myOptions = {
      host: '127.0.0.1',
      port: a.address().port,
      rejectUnauthorized: false
    };
    const socket = tls.connect(myOptions);
    const ssl = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    ssl.setEncoding('utf8');
    let buf = '';
    ssl.on('data', function(data) {
      buf += data;
    });
    ssl.on('end', common.mustCall(function() {
      assert.strictEqual(buf, body);
      ssl.end();
      a.close();
      b.close();
    }));
  });
});
