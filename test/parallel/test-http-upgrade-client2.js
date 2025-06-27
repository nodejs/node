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
const http = require('http');

const CRLF = '\r\n';

const server = http.createServer();
server.on('upgrade', function(req, socket) {
  socket.write(`HTTP/1.1 101 Ok${CRLF}` +
               `Connection: Upgrade${CRLF}` +
               `Upgrade: Test${CRLF}${CRLF}` +
               'head');
  socket.on('end', function() {
    socket.end();
  });
});

server.listen(0, common.mustCall(function() {

  function upgradeRequest(fn) {
    console.log('req');
    const header = { 'Connection': 'Upgrade', 'Upgrade': 'Test' };
    const request = http.request({
      port: server.address().port,
      headers: header
    });
    let wasUpgrade = false;

    function onUpgrade(res, socket) {
      console.log('client upgraded');
      wasUpgrade = true;

      request.removeListener('upgrade', onUpgrade);
      socket.end();
    }
    request.on('upgrade', onUpgrade);

    function onEnd() {
      console.log('client end');
      request.removeListener('end', onEnd);
      if (!wasUpgrade) {
        throw new Error('hasn\'t received upgrade event');
      } else {
        fn && process.nextTick(fn);
      }
    }
    request.on('close', onEnd);

    request.write('head');

  }

  upgradeRequest(common.mustCall(function() {
    upgradeRequest(common.mustCall(function() {
      // Test pass
      console.log('Pass!');
      server.close();
    }));
  }));
}));
