'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Destroying a session while a socket write is pending must flush the final
// GOAWAY before ending the socket, including when called from a stream handler.
const assert = require('assert');
const http2 = require('http2');
const { Duplex } = require('stream');
const {
  HeadersFrame,
  SettingsFrame,
  kClientMagic,
  kFakeRequestHeaders,
} = require('../common/http2');

for (const [code, deferDestroy] of [
  [0, false],
  [http2.constants.NGHTTP2_REFUSED_STREAM, false],
  [0, true],
  [http2.constants.NGHTTP2_REFUSED_STREAM, true],
]) {
  const output = [];
  let pendingWrite;
  const socket = new Duplex({
    read() {},
    write(chunk, encoding, callback) {
      output.push(Buffer.from(chunk));
      if (pendingWrite === undefined) {
        // Hold the initial SETTINGS write until the stream handler destroys
        // the session. No timers or large response bodies are needed.
        pendingWrite = callback;
        setImmediate(() => {
          socket.push(new HeadersFrame(1, kFakeRequestHeaders, 0, true).data);
        });
      } else {
        callback();
      }
    },
  });

  const session = http2.performServerHandshake(socket);
  session.on('stream', common.mustCall((stream) => {
    assert.strictEqual(typeof pendingWrite, 'function');
    stream.on('close', common.mustCall());
    if (code !== 0) {
      stream.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'ERR_HTTP2_SESSION_ERROR');
      }));
    }
    const destroy = common.mustCall(() => {
      session.destroy(code);
      assert.strictEqual(session.destroyed, true);
      setImmediate(common.mustCall(() => {
        const callback = pendingWrite;
        pendingWrite = null;
        callback();
      }));
    });
    if (deferDestroy)
      setImmediate(destroy);
    else
      destroy();
  }));
  session.on('close', common.mustCall());
  if (code !== 0) {
    session.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_HTTP2_SESSION_ERROR');
    }));
  }

  socket.on('finish', common.mustCall(() => {
    const data = Buffer.concat(output);
    let goaways = 0;
    for (let offset = 0; offset < data.length;) {
      const length = data.readUIntBE(offset, 3);
      assert(data.length >= offset + 9 + length);
      if (data[offset + 3] === 7) {
        goaways++;
        assert.strictEqual(data.readUInt32BE(offset + 9), 1);
        assert.strictEqual(data.readUInt32BE(offset + 13), code);
      }
      offset += 9 + length;
    }
    assert.strictEqual(goaways, 1);
  }));

  socket.push(Buffer.concat([kClientMagic, new SettingsFrame().data]));
}
