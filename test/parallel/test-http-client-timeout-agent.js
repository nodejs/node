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
require('../common');
const assert = require('assert');
const http = require('http');

let requests_sent = 0;
let requests_done = 0;
const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
};

const server = http.createServer((req, res) => {
  const m = /\/(.*)/.exec(req.url);
  const reqid = parseInt(m[1], 10);
  if (reqid % 2) {
    // do not reply the request
  } else {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write(reqid.toString());
    res.end();
  }
});

server.listen(0, options.host, function() {
  options.port = this.address().port;

  for (requests_sent = 0; requests_sent < 30; requests_sent += 1) {
    options.path = `/${requests_sent}`;
    const req = http.request(options);
    req.id = requests_sent;
    req.on('response', (res) => {
      res.on('data', function(data) {
        console.log(`res#${this.req.id} data:${data}`);
      });
      res.on('end', function(data) {
        console.log(`res#${this.req.id} end`);
        requests_done += 1;
        req.destroy();
      });
    });
    req.on('close', function() {
      console.log(`req#${this.id} close`);
    });
    req.on('error', function() {
      console.log(`req#${this.id} error`);
      this.destroy();
    });
    req.setTimeout(50, function() {
      console.log(`req#${this.id} timeout`);
      this.abort();
      requests_done += 1;
    });
    req.end();
  }

  setTimeout(function maybeDone() {
    if (requests_done >= requests_sent) {
      setTimeout(() => {
        server.close();
      }, 100);
    } else {
      setTimeout(maybeDone, 100);
    }
  }, 100);
});

process.on('exit', () => {
  console.error(`done=${requests_done} sent=${requests_sent}`);
  // Check that timeout on http request was not called too much
  assert.strictEqual(requests_done, requests_sent);
});
