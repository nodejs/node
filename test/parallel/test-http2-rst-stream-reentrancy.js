'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const net = require('net');

const PREFACE = Buffer.from('PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n');

function frame(type, flags, sid, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(9);
  header.writeUIntBE(payload.length, 0, 3);
  header[3] = type;
  header[4] = flags;
  header.writeUInt32BE(sid & 0x7fffffff, 5);
  return Buffer.concat([header, payload]);
}

function goaway(lastStreamID, code) {
  const payload = Buffer.alloc(8);
  payload.writeUInt32BE(lastStreamID, 0);
  payload.writeUInt32BE(code, 4);
  return frame(7, 0, 0, payload);
}

function headers(sid) {
  return frame(1, 0x05, sid, Buffer.from([
    0x82,  // :method: GET
    0x86,  // :scheme: http
    0x84,  // :path: /
    0x41, 0x01, 0x78,  // :authority: x
  ]));
}

const server = http2.createServer();

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  let sent = false;

  socket.on('connect', common.mustCall(() => {
    socket.write(Buffer.concat([PREFACE, frame(4, 0, 0)]));
  }));

  socket.on('error', () => {});

  socket.on('data', common.mustCallAtLeast(() => {
    if (sent)
      return;
    sent = true;

    socket.write(frame(4, 1, 0));
    socket.write(headers(1));

    setImmediate(() => {
      socket.write(Buffer.concat([
        goaway(0, 0),
        headers(3),
        headers(5),
        headers(7),
      ]));
      setTimeout(() => socket.destroy(), common.platformTimeout(50));
    });
  }));

  socket.on('close', common.mustCall(() => server.close()));
}));
