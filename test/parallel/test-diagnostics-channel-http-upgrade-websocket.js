'use strict';

const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http = require('http');
const net = require('net');

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

const serverTextFrame = encodeFrame(0x1, 'hello');
const serverPingFrame = encodeFrame(0x9, Buffer.from('!'));
const serverBinaryFrame = encodeFrame(0x2, Buffer.from([1, 2, 3]));

const clientTextFrame = encodeFrame(0x1, 'hel', {
  fin: false,
  masked: true,
  mask: Buffer.from([5, 6, 7, 8]),
});
const clientContinuationFrame = encodeFrame(0x0, 'lo', {
  masked: true,
  mask: Buffer.from([9, 10, 11, 12]),
});
const clientCloseFrame = encodeFrame(0x8, Buffer.alloc(0), {
  masked: true,
  mask: Buffer.from([13, 14, 15, 16]),
});

let request;
let upgradeEvent;
let upgradedSocket;
let upgradeResponse;

const receivedMessages = [];
const sentMessages = [];
const receivedFrames = [];
const sentFrames = [];

dc.subscribe('http.client.request.upgrade', common.mustCall((message) => {
  upgradeEvent = message;
  assert.strictEqual(message.request, request);
  assert.strictEqual(message.protocol, 'websocket');
  assert.ok(message.head instanceof Buffer);
}, 1));

dc.subscribe('websocket.client.frameReceived', common.mustCall((message) => {
  assert.strictEqual(message.request, request);
  receivedMessages.push(message);
  receivedFrames.push({
    opcode: message.opcode,
    fin: message.fin,
    masked: message.masked,
    compressed: message.compressed,
    payloadLength: message.payloadLength,
    payload: Buffer.from(message.payload),
  });
}, 3));

dc.subscribe('websocket.client.frameSent', common.mustCall((message) => {
  assert.strictEqual(message.request, request);
  sentMessages.push(message);
  sentFrames.push({
    opcode: message.opcode,
    fin: message.fin,
    masked: message.masked,
    compressed: message.compressed,
    payloadLength: message.payloadLength,
    payload: Buffer.from(message.payload),
  });
}, 3));

const server = net.createServer(common.mustCall((socket) => {
  let requestBuffer = Buffer.alloc(0);

  socket.on('data', (chunk) => {
    requestBuffer = Buffer.concat([requestBuffer, chunk]);
    if (!requestBuffer.includes('\r\n\r\n')) {
      return;
    }

    socket.removeAllListeners('data');

    const response = Buffer.from(
      'HTTP/1.1 101 Switching Protocols\r\n' +
      'Connection: Upgrade\r\n' +
      'Upgrade: websocket\r\n' +
      '\r\n',
      'latin1',
    );

    socket.write(Buffer.concat([
      response,
      serverTextFrame.subarray(0, 4),
    ]));

    setImmediate(() => {
      socket.write(Buffer.concat([
        serverTextFrame.subarray(4),
        serverPingFrame,
        serverBinaryFrame,
      ]));

      setTimeout(() => socket.end(), 20);
    });
  });
}), 1);

server.listen(0, common.mustCall(() => {
  request = http.get({
    port: server.address().port,
    headers: {
      Connection: 'Upgrade',
      Upgrade: 'websocket',
    },
  });

  request.on('error', common.mustNotCall());
  request.on('upgrade', common.mustCall((response, socket) => {
    upgradeResponse = response;
    upgradedSocket = socket;

    socket.on('error', common.mustNotCall());
    socket.resume();

    socket.write(clientTextFrame.subarray(0, 2));
    socket.write(clientTextFrame.subarray(2));
    socket.write(Buffer.concat([
      clientContinuationFrame,
      clientCloseFrame,
    ]));

    socket.on('close', common.mustCall(() => {
      assert.strictEqual(upgradeEvent.response, upgradeResponse);
      assert.strictEqual(upgradeEvent.socket, upgradedSocket);
      assert.ok(upgradeEvent.head.byteLength >= 4);
      assert.deepStrictEqual(
        upgradeEvent.head.subarray(0, 4),
        serverTextFrame.subarray(0, 4),
      );
      assert.deepStrictEqual(receivedMessages.map((message) => message.response), [
        upgradeResponse,
        upgradeResponse,
        upgradeResponse,
      ]);
      assert.deepStrictEqual(receivedMessages.map((message) => message.socket), [
        upgradedSocket,
        upgradedSocket,
        upgradedSocket,
      ]);
      assert.deepStrictEqual(sentMessages.map((message) => message.response), [
        upgradeResponse,
        upgradeResponse,
        upgradeResponse,
      ]);
      assert.deepStrictEqual(sentMessages.map((message) => message.socket), [
        upgradedSocket,
        upgradedSocket,
        upgradedSocket,
      ]);

      assert.deepStrictEqual(receivedFrames, [
        {
          opcode: 0x1,
          fin: true,
          masked: false,
          compressed: false,
          payloadLength: 5,
          payload: Buffer.from('hello'),
        },
        {
          opcode: 0x9,
          fin: true,
          masked: false,
          compressed: false,
          payloadLength: 1,
          payload: Buffer.from('!'),
        },
        {
          opcode: 0x2,
          fin: true,
          masked: false,
          compressed: false,
          payloadLength: 3,
          payload: Buffer.from([1, 2, 3]),
        },
      ]);

      assert.deepStrictEqual(sentFrames, [
        {
          opcode: 0x1,
          fin: false,
          masked: true,
          compressed: false,
          payloadLength: 3,
          payload: Buffer.from('hel'),
        },
        {
          opcode: 0x0,
          fin: true,
          masked: true,
          compressed: false,
          payloadLength: 2,
          payload: Buffer.from('lo'),
        },
        {
          opcode: 0x8,
          fin: true,
          masked: true,
          compressed: false,
          payloadLength: 0,
          payload: Buffer.alloc(0),
        },
      ]);

      server.close();
    }));
  }));
}));
