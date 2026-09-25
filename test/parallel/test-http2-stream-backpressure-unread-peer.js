'use strict';

// CVE-2019-9517 describes a peer that offers HTTP/2 flow-control credit
// but then doesn't read from its TCP connection (blocking TCP flow).
// A streaming response must stop producing data when the resulting server
// write becomes blocked by the transport.

// We use a raw client here to directly control the flow of requests and
// stall responses to deterministically reproduce this on all platforms.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { Duplex, Readable } = require('stream');

const { MAX_INITIAL_WINDOW_SIZE } = http2.constants;

const FRAME_HEADERS = 1;
const FRAME_SETTINGS = 4;
const FRAME_WINDOW_UPDATE = 8;
const FLAG_END_STREAM = 1;
const FLAG_END_HEADERS = 4;
const INITIAL_CONNECTION_WINDOW_SIZE = 65_535;

const preface = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

function frame(type, flags, streamID, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(streamID, 5);
  return Buffer.concat([header, payload]);
}

function settings() {
  const payload = http2.getPackedSettings({
    initialWindowSize: MAX_INITIAL_WINDOW_SIZE,
  });
  return frame(FRAME_SETTINGS, 0, 0, payload);
}

function connectionWindowUpdate() {
  const payload = Buffer.alloc(4);
  payload.writeUInt32BE(
    MAX_INITIAL_WINDOW_SIZE - INITIAL_CONNECTION_WINDOW_SIZE, 0);
  return frame(FRAME_WINDOW_UPDATE, 0, 0, payload);
}

function request() {
  return frame(
    FRAME_HEADERS,
    FLAG_END_HEADERS | FLAG_END_STREAM,
    1,
    Buffer.from([
      0x82,  // :method: GET
      0x86,  // :scheme: http
      0x84,  // :path: /
      0x41, 0x01, 0x78,  // :authority: x
    ]));
}

let onWriteStalled;
const socket = new Duplex({
  read() {},
  write(_data, _encoding, callback) {
    if (onWriteStalled === undefined) {
      callback();
      return;
    }

    // Leave the write pending to model a peer that has stopped reading.
    onWriteStalled();
  },
});

// The 1MB body is well within the client's stream and connection windows
// (2^31-1), so HTTP/2 flow control will not limit the response.
const CHUNK_SIZE = 16 * 1024;
const RESPONSE_SIZE = 1024 * 1024;
const chunk = Buffer.alloc(CHUNK_SIZE);

const serverSession = http2.performServerHandshake(socket);
serverSession.on('stream', common.mustCall((stream) => {
  let produced = 0;

  // A datasource that produces chunks on demand
  const source = new Readable({
    highWaterMark: CHUNK_SIZE,
    read() {
      if (produced === RESPONSE_SIZE) {
        this.push(null);
        return;
      }
      produced += CHUNK_SIZE;
      this.push(chunk);
    },
  });

  // We set this once the stream opens, to stall all future writes once the
  // initial preface & SETTINGS dance is completed.
  onWriteStalled = common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      // Backpressure should have been applied to the source when the
      // socket writable writes stalled, even though there is still
      // H2 flow-control credit available:
      assert(source.isPaused());
      assert(stream.writableNeedDrain);

      // Exact amount produced depends on platform-specific variables but
      // backpressure must stop the source well before it completes:
      assert.ok(produced <= RESPONSE_SIZE / 4,
                `produced ${produced} of ${RESPONSE_SIZE} bytes`);

      serverSession.destroy();
      socket.destroy();
      source.destroy();
    }));
  });

  stream.respond();
  source.pipe(stream);
}));

// Manually send a single request with all required preamble:
socket.push(Buffer.concat([
  preface,
  settings(),
  connectionWindowUpdate(),
  request(),
]));
