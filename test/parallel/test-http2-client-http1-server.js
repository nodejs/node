'use strict';
// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http = require('http');
const http2 = require('http2');
const { NghttpError } = require('internal/http2/util');

// Creating an http1 server here...
const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.on('close', common.mustCall());

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    constructor: NghttpError,
    message: 'Protocol error'
  }));

  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    constructor: NghttpError,
    name: 'Error',
    message: 'Protocol error'
  }));

  client.on('close', common.mustCall(() => server.close()));
}));
