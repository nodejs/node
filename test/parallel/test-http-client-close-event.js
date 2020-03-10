'use strict';
const common = require('../common');

// This test ensures that the `'close'` event is emitted after the `'error'`
// event when a request is made and the socket is closed before we started to
// receive a response.

const http = require('http');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const req = http.get({ port: server.address().port }, common.mustNotCall());

  req.on('error', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    server.close();
  }));

  req.destroy();
}));
