'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const client = dgram.createSocket('udp4');

const toSend = [Buffer.alloc(256, 'x'),
                Buffer.alloc(256, 'y'),
                Buffer.alloc(256, 'z'),
                'hello'];

const received = [];
let totalBytesSent = 0;
let totalBytesReceived = 0;
const arrayBufferViewsCount = common.getArrayBufferViews(
  Buffer.from('')
).length;

client.on('listening', common.mustCall(() => {
  const port = client.address().port;

  client.send(toSend[0], 0, toSend[0].length, port);
  client.send(toSend[1], port);
  client.send([toSend[2]], port);
  client.send(toSend[3], 0, toSend[3].length, port);

  totalBytesSent += toSend.map((buf) => buf.length)
                      .reduce((a, b) => a + b, 0);

  for (const msgBuf of common.getArrayBufferViews(toSend[0])) {
    client.send(msgBuf, 0, msgBuf.byteLength, port);
    totalBytesSent += msgBuf.byteLength;
  }
  for (const msgBuf of common.getArrayBufferViews(toSend[1])) {
    client.send(msgBuf, port);
    totalBytesSent += msgBuf.byteLength;
  }
  for (const msgBuf of common.getArrayBufferViews(toSend[2])) {
    client.send([msgBuf], port);
    totalBytesSent += msgBuf.byteLength;
  }
}));

client.on('message', common.mustCall((buf, info) => {
  received.push(buf.toString());
  totalBytesReceived += info.size;

  if (totalBytesReceived === totalBytesSent) {
    client.close();
  }
  // For every buffer in `toSend`, we send the raw Buffer,
  // as well as every TypedArray in getArrayBufferViews()
}, toSend.length + (toSend.length - 1) * arrayBufferViewsCount));

client.on('close', common.mustCall((buf, info) => {
  // The replies may arrive out of order -> sort them before checking.
  received.sort();

  const repeated = [...toSend];
  for (let i = 0; i < arrayBufferViewsCount; i++) {
    repeated.push(...toSend.slice(0, 3));
  }

  assert.strictEqual(totalBytesSent, totalBytesReceived);

  const expected = repeated.map(String).sort();
  assert.deepStrictEqual(received, expected);
}));

client.bind(0);
