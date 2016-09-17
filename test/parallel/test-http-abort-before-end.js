'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.skip);

server.listen(0, common.mustCall(function() {
  const req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: this.address().port
  });

  req.write('foo=bar');

  req.on('error', common.mustCall((ex) => {
    // https://github.com/joyent/node/issues/1399#issuecomment-2597359
    // abort() should emit an Error, not the net.Socket object
    assert(ex instanceof Error);
  }));

  server.once('request', () => {
    req.abort();
    req.end();

    server.close();
  });
}));
