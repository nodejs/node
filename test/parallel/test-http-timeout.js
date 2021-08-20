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
const Countdown = require('../common/countdown');
const MAX_COUNT = 11;

const server = http.createServer(common.mustCall(function(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('OK');
}, MAX_COUNT));

const agent = new http.Agent({ maxSockets: 1 });
const countdown = new Countdown(MAX_COUNT, () => server.close());

server.listen(0, function() {

  for (let i = 0; i < MAX_COUNT; ++i) {
    createRequest().end();
  }

  function callback() {}

  function createRequest() {
    const req = http.request(
      { port: server.address().port, path: '/', agent: agent },
      function(res) {
        req.clearTimeout(callback);

        res.on('end', function() {
          countdown.dec();
        });

        res.resume();
      }
    );

    req.setTimeout(1000, callback);
    return req;
  }
});
