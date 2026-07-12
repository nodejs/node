'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const EventEmitter = require('events');

// This test ensures that a MaxListenersExceededWarning isn't emitted if
// more than EventEmitter.defaultMaxListeners requests are started on a
// ClientHttp2Session before it has finished connecting.

process.on('warning', common.mustNotCall('A warning was emitted'));

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond();
  stream.end();
});

server.listen(common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  function request() {
    return new Promise((resolve, reject) => {
      const stream = client.request();
      stream.on('error', reject);
      stream.on('response', resolve);
      stream.end();
    });
  }

  const requests = [];
  for (let i = 0; i < EventEmitter.defaultMaxListeners + 1; i++) {
    requests.push(request());
  }

  Promise.all(requests).then(common.mustCall()).finally(common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
