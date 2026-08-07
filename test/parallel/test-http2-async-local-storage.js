'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const async_hooks = require('async_hooks');

const storage = new async_hooks.AsyncLocalStorage();

const {
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_STATUS,
} = http2.constants;

function runTest(serverHandler) {
  const server = http2.createServer();
  server.on('stream', serverHandler);

  server.listen(0, common.mustCall(() => {
    const client = storage.run({ id: 0 }, () =>
      http2.connect(`http://localhost:${server.address().port}`));

    let ended = 0;
    function doReq(id) {
      const req = client.request({ [HTTP2_HEADER_PATH]: '/' });

      req.on('response', common.mustCall((headers) => {
        assert.strictEqual(headers[HTTP2_HEADER_STATUS], 200);
        assert.strictEqual(storage.getStore().id, id);
      }));
      req.on('data', common.mustCall(() => {
        assert.strictEqual(storage.getStore().id, id);
      }));
      req.on('end', common.mustCall(() => {
        assert.strictEqual(storage.getStore().id, id);
        if (++ended === 2) {
          server.close();
          client.close();
        }
      }));
    }

    storage.run({ id: 1 }, () => doReq(1));
    storage.run({ id: 2 }, () => doReq(2));
  }));
}

// Response without trailers: END_STREAM on the DATA frame.
runTest((stream) => {
  stream.respond({ [HTTP2_HEADER_STATUS]: 200 });
  stream.end('data');
});

// Response with trailers (as gRPC uses): END_STREAM on the trailing HEADERS
// frame. The 'end' event must still run in the request's context.
runTest((stream) => {
  stream.respond({ [HTTP2_HEADER_STATUS]: 200 }, { waitForTrailers: true });
  stream.on('wantTrailers', () => {
    stream.sendTrailers({ 'grpc-status': '0' });
  });
  stream.write('data');
  stream.end();
});
