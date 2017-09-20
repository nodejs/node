// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
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

  const client = h2.connect(`http://localhost:${server.address().port}`);

  assert.throws(() => client.settings({ headerTableSize: -1 }),
                RangeError);
  assert.throws(() => client.settings({ headerTableSize: 2 ** 32 }),
                RangeError);
  assert.throws(() => client.settings({ initialWindowSize: -1 }),
                RangeError);
  assert.throws(() => client.settings({ initialWindowSize: 2 ** 32 }),
                RangeError);
  assert.throws(() => client.settings({ maxFrameSize: 1 }),
                RangeError);
  assert.throws(() => client.settings({ maxFrameSize: 2 ** 24 }),
                RangeError);
  assert.throws(() => client.settings({ maxConcurrentStreams: -1 }),
                RangeError);
  assert.throws(() => client.settings({ maxConcurrentStreams: 2 ** 31 }),
                RangeError);
  assert.throws(() => client.settings({ maxHeaderListSize: -1 }),
                RangeError);
  assert.throws(() => client.settings({ maxHeaderListSize: 2 ** 32 }),
                RangeError);
  ['a', 1, 0, null, {}].forEach((i) => {
    assert.throws(() => client.settings({ enablePush: i }), TypeError);
  });

  client.settings({ maxFrameSize: 1234567 });

  const req = client.request({ ':path': '/' });

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
