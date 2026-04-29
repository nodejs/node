'use strict';

const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http = require('http');
const net = require('net');
const stream = require('stream');
const { once } = require('events');

function encodeFrame(opcode, payload, {
  fin = true,
  masked = false,
  mask = Buffer.from([1, 2, 3, 4]),
} = {}) {
  payload = typeof payload === 'string' ? Buffer.from(payload) : payload;

  const header = [
    (fin ? 0x80 : 0) | opcode,
  ];

  if (payload.byteLength < 126) {
    header.push((masked ? 0x80 : 0) | payload.byteLength);
  } else {
    throw new Error('payload too large for test');
  }

  if (!masked) {
    return Buffer.concat([Buffer.from(header), payload]);
  }

  const maskedPayload = Buffer.from(payload);
  for (let i = 0; i < maskedPayload.byteLength; i++) {
    maskedPayload[i] ^= mask[i % 4];
  }

  return Buffer.concat([Buffer.from(header), mask, maskedPayload]);
}

async function runClient(port, requestHead, {
  initialTail,
  secondWrite,
  closeFrame,
}) {
  const socket = net.createConnection({
    host: '127.0.0.1',
    port,
  });

  let responseBuffer = Buffer.alloc(0);
  let responseReceived = false;
  let secondWriteSent = secondWrite === undefined;

  function maybeClose() {
    if (responseReceived && secondWriteSent) {
      socket.end(closeFrame);
    }
  }

  socket.on('data', (chunk) => {
    responseBuffer = Buffer.concat([responseBuffer, chunk]);
    if (!responseReceived && responseBuffer.includes('\r\n\r\n')) {
      responseReceived = true;
      maybeClose();
    }
  });

  socket.on('error', common.mustNotCall());
  await once(socket, 'connect');

  const headers = Buffer.from(requestHead, 'latin1');

  if (initialTail !== undefined) {
    socket.write(Buffer.concat([headers, initialTail]));
  } else {
    socket.write(headers);
  }

  if (secondWrite !== undefined) {
    setImmediate(() => {
      socket.write(secondWrite);
      secondWriteSent = true;
      maybeClose();
    });
  }

  await once(socket, 'close');
}

const serverTextFrame = encodeFrame(0x1, 'server');
const completeHeadFrame = encodeFrame(0x1, 'complete', {
  masked: true,
  mask: Buffer.from([5, 6, 7, 8]),
});
const incompleteBodyFrame = encodeFrame(0x1, 'incomplete', {
  masked: true,
  mask: Buffer.from([9, 10, 11, 12]),
});
const closeFrame = encodeFrame(0x8, Buffer.alloc(0), {
  masked: true,
  mask: Buffer.from([13, 14, 15, 16]),
});

const upgradeEvents = [];
const receivedFrames = [];
const sentFrames = [];

dc.subscribe('http.server.request.upgrade', common.mustCall((message) => {
  upgradeEvents.push({
    url: message.request.url,
    socketOrStream: message.socketOrStream,
    head: Buffer.from(message.head),
    protocol: message.protocol,
  });
}, 2));

dc.subscribe('websocket.server.frameReceived', common.mustCall((message) => {
  receivedFrames.push({
    url: message.request.url,
    socket: message.socket,
    opcode: message.opcode,
    fin: message.fin,
    masked: message.masked,
    compressed: message.compressed,
    payloadLength: message.payloadLength,
    payload: Buffer.from(message.payload),
  });
}, 4));

dc.subscribe('websocket.server.frameSent', common.mustCall((message) => {
  sentFrames.push({
    url: message.request.url,
    socket: message.socket,
    opcode: message.opcode,
    fin: message.fin,
    masked: message.masked,
    compressed: message.compressed,
    payloadLength: message.payloadLength,
    payload: Buffer.from(message.payload),
  });
}, 2));

const server = http.createServer(common.mustNotCall());
server.on('upgrade', common.mustCall((request, socketOrStream) => {
  socketOrStream.on('error', common.mustNotCall());
  socketOrStream.resume();
  socketOrStream.write(
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Connection: Upgrade\r\n' +
    'Upgrade: websocket\r\n' +
    '\r\n',
  );
  socketOrStream.write(serverTextFrame);
  socketOrStream.on('end', () => socketOrStream.end());
}, 2));

server.listen(0, '127.0.0.1', common.mustCall(async () => {
  const port = server.address().port;

  await runClient(port,
                  'POST /complete HTTP/1.1\r\n' +
                  'Host: localhost\r\n' +
                  'Connection: Upgrade\r\n' +
                  'Upgrade: websocket\r\n' +
                  'Content-Length: 0\r\n' +
                  '\r\n', {
                    initialTail: completeHeadFrame,
                    closeFrame,
                  });

  await runClient(port,
                  'POST /incomplete HTTP/1.1\r\n' +
                  'Host: localhost\r\n' +
                  'Connection: Upgrade\r\n' +
                  'Upgrade: websocket\r\n' +
                  'Transfer-Encoding: chunked\r\n' +
                  'Trailer: X-Test\r\n' +
                  '\r\n', {
                    secondWrite: Buffer.concat([
                      Buffer.from('4\r\nbody\r\n0\r\nX-Test: yes\r\n\r\n', 'latin1'),
                      incompleteBodyFrame,
                    ]),
                    closeFrame,
                  });

  server.close();
}));

process.on('exit', () => {
  assert.deepStrictEqual(upgradeEvents.map(({ url, protocol }) => ({ url, protocol })), [
    { url: '/complete', protocol: 'websocket' },
    { url: '/incomplete', protocol: 'websocket' },
  ]);
  assert.strictEqual(upgradeEvents[0].socketOrStream instanceof net.Socket, true);
  assert.strictEqual(upgradeEvents[1].socketOrStream instanceof stream.Duplex, true);
  assert.deepStrictEqual(upgradeEvents[0].head, completeHeadFrame);
  assert.ok(upgradeEvents[1].head.byteLength >= 0);

  assert.deepStrictEqual(receivedFrames.map((frame) => ({
    url: frame.url,
    opcode: frame.opcode,
    fin: frame.fin,
    masked: frame.masked,
    compressed: frame.compressed,
    payloadLength: frame.payloadLength,
    payload: frame.payload,
  })), [
    {
      url: '/complete',
      opcode: 0x1,
      fin: true,
      masked: true,
      compressed: false,
      payloadLength: 8,
      payload: Buffer.from('complete'),
    },
    {
      url: '/complete',
      opcode: 0x8,
      fin: true,
      masked: true,
      compressed: false,
      payloadLength: 0,
      payload: Buffer.alloc(0),
    },
    {
      url: '/incomplete',
      opcode: 0x1,
      fin: true,
      masked: true,
      compressed: false,
      payloadLength: 10,
      payload: Buffer.from('incomplete'),
    },
    {
      url: '/incomplete',
      opcode: 0x8,
      fin: true,
      masked: true,
      compressed: false,
      payloadLength: 0,
      payload: Buffer.alloc(0),
    },
  ]);

  assert.deepStrictEqual(sentFrames.map((frame) => ({
    url: frame.url,
    opcode: frame.opcode,
    fin: frame.fin,
    masked: frame.masked,
    compressed: frame.compressed,
    payloadLength: frame.payloadLength,
    payload: frame.payload,
  })), [
    {
      url: '/complete',
      opcode: 0x1,
      fin: true,
      masked: false,
      compressed: false,
      payloadLength: 6,
      payload: Buffer.from('server'),
    },
    {
      url: '/incomplete',
      opcode: 0x1,
      fin: true,
      masked: false,
      compressed: false,
      payloadLength: 6,
      payload: Buffer.from('server'),
    },
  ]);
});
