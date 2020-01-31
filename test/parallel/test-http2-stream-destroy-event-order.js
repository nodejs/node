'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

let client;
let req;
const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.mustCall(() => {
    stream.on('close', common.mustCall(() => {
      server.close();
    }));
  }));

  req.close(2);
}));
server.listen(0, common.mustCall(() => {
  client = http2.connect(`http://localhost:${server.address().port}`);
  req = client.request();
  req.resume();
  req.on('error', common.mustCall(() => {
    req.on('close', common.mustCall(() => {
      // TODO(lundibundi): fix flakiness of this use case, the setTimeout must
      // not be needed here. Specifically if the settings frame on the client
      // is 'delayed' and only comes after we have already called
      // `req.close(2)` then the 'close' event will be emitted before the
      // stream has got a chance to write the rst to the underlying socket
      // (even though FlushRstStream and SendPendingData have been called) and
      // since we are destroying a session here it will never be written
      // resulting in a 'clean' stream close on the server side and therefore a
      // timeout because there will be no error.
      setTimeout(() => client.close(), 0);
    }));
  }));
}));
