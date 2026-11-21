// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const { isBoringSSL } = require('../common/crypto');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');

const buf = Buffer.allocUnsafe(10000);
let received = 0;
const maxChunk = 768;
// Largest application_data record the server sent, as seen on the wire.
let maxRecord = 0;

const invalidArgumentError = {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE'
};

// TLS 1.2, so that application_data records are not confused with the
// encrypted handshake records of TLS 1.3.
const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  maxVersion: 'TLSv1.2',
}, common.mustCall((c) => {

  // No size is passed.
  assert.throws(() => c.setMaxSendFragment(), invalidArgumentError);

  // Invalid arg is passed.
  [null, undefined, '', {}, false, true, []].forEach((arg) => {
    assert.throws(() => c.setMaxSendFragment(arg), invalidArgumentError);
  });

  [NaN, Infinity, 2 ** 31].forEach((arg) => {
    assert.throws(() => c.setMaxSendFragment(arg), {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE'
    });
  });

  assert.throws(() => c.setMaxSendFragment(Symbol()), { name: 'TypeError' });

  // OpenSSL enforces Node's documented fragment size range. BoringSSL accepts
  // both out-of-range values and reports success, so assert that difference
  // explicitly instead of using a truthiness shortcut.
  const acceptsOutOfRangeFragmentSize =
    isBoringSSL;
  assert.strictEqual(c.setMaxSendFragment(511),
                     acceptsOutOfRangeFragmentSize);
  assert.strictEqual(c.setMaxSendFragment(16385),
                     acceptsOutOfRangeFragmentSize);

  // Correct fragment size.
  assert(c.setMaxSendFragment(maxChunk));

  c.end(buf);
})).listen(0, common.mustCall(() => {
  // Records that arrive together are emitted as one 'data' chunk, so the
  // fragment size is checked on the wire: a proxy parses the TLS record
  // headers of everything the server sends.
  const proxy = net.createServer(common.mustCall((socket) => {
    const upstream = net.connect(server.address().port);
    socket.pipe(upstream);
    let pending = Buffer.alloc(0);
    upstream.on('data', (data) => {
      socket.write(data);
      pending = Buffer.concat([pending, data]);
      while (pending.length >= 5) {
        const length = pending.readUInt16BE(3);
        if (pending.length < 5 + length)
          break;
        if (pending[0] === 23)  // application_data
          maxRecord = Math.max(maxRecord, length);
        pending = pending.subarray(5 + length);
      }
    });
    upstream.on('end', () => socket.end());
  })).listen(0, common.mustCall(() => {
    const c = tls.connect(proxy.address().port, {
      rejectUnauthorized: false
    }, common.mustCall(() => {
      c.on('data', common.mustCallAtLeast((chunk) => {
        received += chunk.length;
      }));

      // Ensure that we receive 'end' event anyway
      c.on('end', common.mustCall(() => {
        c.destroy();
        proxy.close();
        server.close();
        assert.strictEqual(received, buf.length);
        // A record carries at most maxChunk bytes of data plus the cipher's
        // overhead (IV, MAC or tag, padding).
        assert(maxRecord > 0 && maxRecord <= maxChunk + 256,
               `largest record ${maxRecord}`);
      }));
    }));
  }));
}));
