'use strict';
// Serving up a zero-length buffer should work.

const common = require('../common');
const http = require('http');

const server = http.createServer((req, res) => {
  const buffer = Buffer.alloc(0);
  // FIXME: WTF gjslint want this?
  res.writeHead(200, { 'Content-Type': 'text/html',
                       'Content-Length': buffer.length });
  res.end(buffer);
});

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {

    res.on('data', common.mustNotCall());

    res.on('end', (d) => {
      server.close();
    });
  }));
}));
