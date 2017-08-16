// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const { PADDING_STRATEGY_CALLBACK } = h2.constants;

function selectPadding(frameLen, max) {
  assert.strictEqual(typeof frameLen, 'number');
  assert.strictEqual(typeof max, 'number');
  assert(max >= frameLen);
  return max;
}

// selectPadding will be called three times:
// 1. For the client request headers frame
// 2. For the server response headers frame
// 3. For the server response data frame
const options = {
  paddingStrategy: PADDING_STRATEGY_CALLBACK,
  selectPadding: common.mustCall(selectPadding, 3)
};

const server = h2.createServer(options);
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`,
                            options);

  const req = client.request({ ':path': '/' });
  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();
}));
