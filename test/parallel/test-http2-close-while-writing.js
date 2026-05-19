'use strict';
// https://github.com/nodejs/node/issues/33156
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const http2 = require('http2');

const key = fixtures.readKey('agent8-key.pem', 'binary');
const cert = fixtures.readKey('agent8-cert.pem', 'binary');
const ca = fixtures.readKey('fake-startcom-root-cert.pem', 'binary');

const server = http2.createSecureServer({
  key,
  cert,
  maxSessionMemory: 1000
});

let client_stream;

server.on('session', common.mustCall(function(session) {
  session.on('stream', common.mustCall(function(stream) {
    let writes = 0;
    let writeCallbacks = 0;

    stream.resume();
    stream.on('data', common.mustCallAtLeast(function() {
      writes++;
      this.write(Buffer.alloc(1), common.mustCall(() => {
        writeCallbacks++;
      }));
      process.nextTick(() => client_stream.destroy());
    }));
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(writeCallbacks, writes);
    }));
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`, {
    ca,
    maxSessionMemory: 1000
  });
  client_stream = client.request({ ':method': 'POST' });
  client_stream.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
  client_stream.resume();
  client_stream.write(Buffer.alloc(64 * 1024));
}));
