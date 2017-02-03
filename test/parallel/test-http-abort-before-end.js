'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustNotCall());

server.listen(0, function() {
  const req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: this.address().port
  });

  req.on('error', function(ex) {
    // https://github.com/joyent/node/issues/1399#issuecomment-2597359
    // abort() should emit an Error, not the net.Socket object
    assert(ex instanceof Error);
  });

  req.abort();
  req.end();

  server.close();
});
