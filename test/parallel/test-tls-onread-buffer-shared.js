'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// An `onread` buffer backed by a `SharedArrayBuffer` can never be detached.
// Reading into one must not throw from the detached-buffer check.

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
};

const message = Buffer.from('hello world');

function test(buffer, check) {
  tls.createServer(options, common.mustCall(function(socket) {
    this.close();
    socket.end(message);
  })).listen(0, common.mustCall(function() {
    const incoming = [];
    tls.connect({
      port: this.address().port,
      rejectUnauthorized: false,
      onread: {
        buffer,
        callback: common.mustCallAtLeast((nread, buf) => {
          check(buf);
          assert.ok(buf.buffer instanceof SharedArrayBuffer);
          incoming.push(Buffer.from(buf.subarray(0, nread)));
        }),
      },
    }).on('data', common.mustNotCall())
      .on('error', common.mustNotCall())
      .on('end', common.mustCall(() => {
        assert.deepStrictEqual(Buffer.concat(incoming), message);
      }));
  }));
}

// Static buffer.
{
  const sockBuf = new Uint8Array(new SharedArrayBuffer(4));
  test(sockBuf, (buf) => assert.strictEqual(buf, sockBuf));
}

// Buffer returned by a factory function.
{
  let last;
  test(common.mustCallAtLeast(() => {
    last = new Uint8Array(new SharedArrayBuffer(4));
    return last;
  }), (buf) => assert.strictEqual(buf, last));
}
