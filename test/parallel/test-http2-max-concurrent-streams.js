// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_PATH,
  HTTP2_METHOD_POST
} = h2.constants;

// Only allow one stream to be open at a time
const server = h2.createServer({ settings: { maxConcurrentStreams: 1 } });

// The stream handler must be called only once
server.on('stream', common.mustCall((stream) => {
  stream.respond({ [HTTP2_HEADER_STATUS]: 200 });
  stream.end('hello world');
}));
server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  let reqs = 2;
  function onEnd() {
    if (--reqs === 0) {
      server.close();
      client.destroy();
    }
  }

  client.on('remoteSettings', common.mustCall((settings) => {
    assert.strictEqual(settings.maxConcurrentStreams, 1);
  }));

  // This one should go through with no problems
  const req1 = client.request({
    [HTTP2_HEADER_PATH]: '/',
    [HTTP2_HEADER_METHOD]: HTTP2_METHOD_POST
  });
  req1.on('aborted', common.mustNotCall());
  req1.on('response', common.mustCall());
  req1.resume();
  req1.on('end', onEnd);
  req1.end();

  // This one should be aborted
  const req2 = client.request({
    [HTTP2_HEADER_PATH]: '/',
    [HTTP2_HEADER_METHOD]: HTTP2_METHOD_POST
  });
  req2.on('aborted', common.mustCall());
  req2.on('response', common.mustNotCall());
  req2.resume();
  req2.on('end', onEnd);
  req2.on('error', common.mustCall(common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 7'
  })));

}));
