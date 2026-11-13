'use strict';

// Regression test for https://github.com/nodejs/node/issues/66525.
// A compat API response to a request without a body must end with
// END_STREAM, even if the connection flow-control window is exhausted
// when the empty END_STREAM DATA frame is submitted. Previously the
// stream was reset with RST_STREAM(NO_ERROR) and the queued END_STREAM
// frame was dropped.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const net = require('net');

const kData = 0;
const kHeaders = 1;
const kRstStream = 3;
const kSettings = 4;
const kWindowUpdate = 8;
const kFlagEndStream = 0x1;
const kFlagAck = 0x1;
const kFlagsEndStreamEndHeaders = 0x5;

function frame(type, flags, id, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(id, 5);
  return Buffer.concat([header, payload]);
}

// Minimal HPACK encoding of a GET request for `path`: indexed :method GET,
// indexed :scheme http, and literal :path and :authority without Huffman.
function requestHeaders(path) {
  const literal = (index, value) =>
    Buffer.concat([Buffer.from([index, value.length]), Buffer.from(value)]);
  return Buffer.concat([
    Buffer.from([0x82, 0x86]),
    literal(0x04, path),
    literal(0x01, 'localhost'),
  ]);
}

let client;
let endStreamReceived = false;
let serverStreamClosed = false;

function maybeFinish() {
  if (endStreamReceived && serverStreamClosed) {
    client.destroy();
    server.close();
  }
}

const server = http2.createServer();

server.on('stream', common.mustCall((stream, headers) => {
  if (headers[':path'] === '/small') {
    // The stream must still close on its own once END_STREAM has been sent.
    stream.on('close', common.mustCall(() => {
      serverStreamClosed = true;
      maybeFinish();
    }));
  }
}, 2));

const responses = {};
server.on('request', common.mustCall((req, res) => {
  responses[req.url] = res;
  if (!responses['/small'] || !responses['/big']) return;
  // After /small's body has been written, let /big exhaust the connection
  // window before /small's END_STREAM frame is submitted.
  responses['/small'].stream.once('wantTrailers', () => {
    responses['/big'].end('y'.repeat(1_000_000));
  });
  responses['/small'].end('x'.repeat(1000));
}, 2));

server.listen(0, common.mustCall(() => {
  // A raw client that does not send WINDOW_UPDATE until later, so the
  // connection window stays exhausted once /big has drained it.
  client = net.connect(server.address().port, common.mustCall(() => {
    client.write(Buffer.concat([
      Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'),
      frame(kSettings, 0, 0),
      frame(kHeaders, kFlagsEndStreamEndHeaders, 1, requestHeaders('/big')),
      frame(kHeaders, kFlagsEndStreamEndHeaders, 3, requestHeaders('/small')),
    ]));
  }));

  let buffered = Buffer.alloc(0);
  client.on('data', common.mustCallAtLeast((chunk) => {
    buffered = Buffer.concat([buffered, chunk]);
    while (buffered.length >= 9 &&
           buffered.length >= 9 + buffered.readUIntBE(0, 3)) {
      const length = buffered.readUIntBE(0, 3);
      const type = buffered[3];
      const flags = buffered[4];
      const id = buffered.readUInt32BE(5) & 0x7fffffff;
      buffered = buffered.subarray(9 + length);

      if (type === kSettings && !(flags & kFlagAck))
        client.write(frame(kSettings, kFlagAck, 0));
      if (id !== 3) continue;

      assert.ok(type !== kRstStream || endStreamReceived,
                'stream /small was reset before END_STREAM was sent');
      if ((type === kData || type === kHeaders) && (flags & kFlagEndStream)) {
        endStreamReceived = true;
        maybeFinish();
      }
    }
  }));

  // Reopen the windows later. A correct server sends /small's END_STREAM then.
  setTimeout(common.mustCall(() => {
    const increment = Buffer.alloc(4);
    increment.writeUInt32BE(10_000_000);
    client.write(Buffer.concat([
      frame(kWindowUpdate, 0, 0, increment),
      frame(kWindowUpdate, 0, 1, increment),
    ]));
  }), common.platformTimeout(300));
}));
