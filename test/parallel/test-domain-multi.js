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

  // treat these EE objects as if they are a part of the b domain
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
