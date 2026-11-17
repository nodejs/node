// Flags: --expose-gc
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const net = require('net');

const FRAME_DATA = 0;
const FRAME_HEADERS = 1;
const FRAME_RST_STREAM = 3;
const FRAME_SETTINGS = 4;
const FRAME_WINDOW_UPDATE = 8;
const FLAG_END_STREAM = 1;
const FLAG_END_HEADERS = 4;
const FLAG_PRIORITY = 0x20;
const INITIAL_WINDOW_SIZE = 65_535;
const FIRST_WINDOW_SIZE = 3_000_000;
const SECOND_WINDOW_INCREMENT = 20_000_000;
const LARGE_RESPONSE_SIZE = 30_000_000;
const SMALL_FIRST_BUFFER_SIZE = 80 * 1024;
const SMALL_SECOND_BUFFER_SIZE = 20 * 1024;
const SMALL_RESPONSE_SIZE =
  SMALL_FIRST_BUFFER_SIZE + SMALL_SECOND_BUFFER_SIZE;
const EXPECTED_SMALL_PAYLOAD_SIZE = INITIAL_WINDOW_SIZE;

const preface = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

function frame(type, flags, streamID, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(streamID, 5);
  return Buffer.concat([header, payload]);
}

function windowUpdate(streamID, increment) {
  const payload = Buffer.alloc(4);
  payload.writeUInt32BE(increment);
  return frame(FRAME_WINDOW_UPDATE, 0, streamID, payload);
}

function request(path, streamID, priority = false) {
  const authority = Buffer.from('127.0.0.1');
  let payload = Buffer.concat([
    Buffer.from([
      0x82,  // :method: GET
      0x86,  // :scheme: http
      0x01, authority.length,  // Literal :authority
    ]),
    authority,
    Buffer.from([0x04, path.length]),  // Literal :path
    Buffer.from(path),
  ]);
  let flags = FLAG_END_STREAM | FLAG_END_HEADERS;

  if (priority) {
    const priorityPayload = Buffer.alloc(5);
    // Make this stream exclusively dependent on stream 1 so its DATA is
    // serialized after stream 1's DATA in the pending socket write.
    priorityPayload.writeUInt32BE(0x80000001);
    priorityPayload[4] = 15;
    payload = Buffer.concat([priorityPayload, payload]);
    flags |= FLAG_PRIORITY;
  }

  return frame(FRAME_HEADERS, flags, streamID, payload);
}

const replacementBuffers = [];
let resumeClient;

const server = http2.createServer({ maxSessionMemory: 64 });
server.on('stream', common.mustCall((stream, headers) => {
  const path = headers[':path'];
  stream.on('error', () => {});
  stream.respond();

  if (path === '/A') {
    stream.end(Buffer.alloc(LARGE_RESPONSE_SIZE, 0x41));
    return;
  }

  stream.on('close', common.mustCall(() => {
    setImmediate(() => {
      // Collect the canceled write and reuse allocations of the same size.
      // The pending socket slices must continue to refer to the original data.
      globalThis.gc();
      replacementBuffers.push(...Array.from(
        { length: 32 },
        () => Buffer.alloc(SMALL_FIRST_BUFFER_SIZE, 0x56)));
      // Only now let the backpressured socket write drain.
      resumeClient();
    });
  }));
  // Use _writev() so the sliced first buffer does not carry the request's
  // completion reference. Every buffer must retain a separate owner.
  stream.cork();
  stream.write(Buffer.alloc(SMALL_FIRST_BUFFER_SIZE, 0x53));
  stream.end(Buffer.alloc(SMALL_SECOND_BUFFER_SIZE, 0x53));
}, 2));

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port, common.localhostIPv4);
  let received = Buffer.alloc(0);
  let stream1Received = 0;
  let secondWindowSent = false;
  let resetSent = false;
  const stream3Chunks = [];

  resumeClient = () => socket.resume();

  const finish = common.mustCall((payload) => {
    assert.strictEqual(payload.length, EXPECTED_SMALL_PAYLOAD_SIZE);
    assert.ok(
      payload.equals(Buffer.alloc(EXPECTED_SMALL_PAYLOAD_SIZE, 0x53)),
      'pending DATA used memory from a canceled write');

    socket.destroy();
    server.close(common.mustCall());
  });

  function onStream1Data() {
    if (!secondWindowSent) {
      if (stream1Received < FIRST_WINDOW_SIZE)
        return;
      // Stream 1 has consumed its whole first window, so the server has no
      // DATA left to send. Open stream 3 and the windows together: the server
      // packs the remaining stream 1 DATA and stream 3 DATA (serialized after
      // stream 1) into a single large socket write.
      secondWindowSent = true;
      socket.write(Buffer.concat([
        request('/S', 3, true),
        windowUpdate(0, SECOND_WINDOW_INCREMENT + SMALL_RESPONSE_SIZE),
        windowUpdate(1, SECOND_WINDOW_INCREMENT),
      ]));
      return;
    }

    if (resetSent || stream1Received <= FIRST_WINDOW_SIZE)
      return;

    // Receiving DATA beyond the first window proves the large write has been
    // issued, and stream 3 DATA was packed into it. Stop reading so that write
    // stays pending, then reset stream 3 while its slices are still queued.
    // The server resumes us once the stream is closed and memory is reused.
    resetSent = true;
    socket.pause();
    const payload = Buffer.alloc(4);
    payload.writeUInt32BE(http2.constants.NGHTTP2_CANCEL);
    socket.write(frame(FRAME_RST_STREAM, 0, 3, payload));
  }

  function parseFrames() {
    let offset = 0;
    while (offset + 9 <= received.length) {
      const length = received.readUIntBE(offset, 3);
      if (offset + 9 + length > received.length)
        break;

      const type = received[offset + 3];
      const streamID = received.readUInt32BE(offset + 5) & 0x7fffffff;
      if (type === FRAME_DATA && streamID === 3) {
        stream3Chunks.push(received.subarray(
          offset + 9,
          offset + 9 + length));
      } else if (type === FRAME_DATA && streamID === 1) {
        stream1Received += length;
        onStream1Data();
      }
      offset += 9 + length;
    }
    received = received.subarray(offset);

    const payload = Buffer.concat(stream3Chunks);
    if (payload.length < EXPECTED_SMALL_PAYLOAD_SIZE)
      return;

    finish(payload);
  }

  socket.on('connect', () => {
    socket.write(Buffer.concat([
      preface,
      frame(FRAME_SETTINGS, 0, 0),
      request('/A', 1),
      windowUpdate(0, FIRST_WINDOW_SIZE - INITIAL_WINDOW_SIZE),
      windowUpdate(1, FIRST_WINDOW_SIZE - INITIAL_WINDOW_SIZE),
    ]));
  });

  socket.on('data', (chunk) => {
    received = Buffer.concat([received, chunk]);
    parseFrames();
  });
}));
