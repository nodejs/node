'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

let client;
let req;
const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('close', common.mustCall(() => {
    stream.on('error', common.mustCall(() => {
      server.close();
    }));
  }));

  req.close(2);
}));
server.listen(0, common.mustCall(() => {
  client = http2.connect(`http://localhost:${server.address().port}`);
  req = client.request();
  req.resume();
  req.on('close', common.mustCall(() => {
    req.on('error', common.mustCall(() => {
      client.close();
    }));
  }));
}));
