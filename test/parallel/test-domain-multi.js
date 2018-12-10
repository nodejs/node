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
// Tests of multiple domains happening at once.

const common = require('../common');
const domain = require('domain');
const http = require('http');

const a = domain.create();
a.enter(); // this will be our "root" domain

a.on('error', common.mustNotCall());

const server = http.createServer((req, res) => {
  // child domain of a.
  const b = domain.create();
  a.add(b);

  // Treat these EE objects as if they are a part of the b domain
  // so, an 'error' event on them propagates to the domain, rather
  // than being thrown.
  b.add(req);
  b.add(res);

  b.on('error', common.mustCall((er) => {
    if (res) {
      res.writeHead(500);
      res.end('An error occurred');
    }
    // res.writeHead(500), res.destroy, etc.
    server.close();
  }));

  // XXX this bind should not be necessary.
  // the write cb behavior in http/net should use an
  // event so that it picks up the domain handling.
  res.write('HELLO\n', b.bind(() => {
    throw new Error('this kills domain B, not A');
  }));

}).listen(0, () => {
  const c = domain.create();
  const req = http.get({ host: 'localhost', port: server.address().port });

  // add the request to the C domain
  c.add(req);

  req.on('response', (res) => {
    // add the response object to the C domain
    c.add(res);
    res.pipe(process.stdout);
  });

  c.on('error', common.mustCall((er) => { }));
});
