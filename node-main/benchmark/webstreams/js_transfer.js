'use strict';

const common = require('../common.js');

const { MessageChannel } = require('worker_threads');
const { WritableStream, TransformStream, ReadableStream } = require('stream/web');

const bench = common.createBenchmark(main, {
  payload: ['WritableStream', 'ReadableStream', 'TransformStream'],
  n: [1e4],
});

function main({ n, payload: payloadType }) {
  let createPayload;
  let messages = 0;

  switch (payloadType) {
    case 'WritableStream':
      createPayload = () => new WritableStream();
      break;
    case 'ReadableStream':
      createPayload = () => new ReadableStream();
      break;
    case 'TransformStream':
      createPayload = () => new TransformStream();
      break;
    default:
      throw new Error('Unsupported payload type');
  }

  const { port1, port2 } = new MessageChannel();

  port2.onmessage = onMessage;

  function onMessage() {
    if (messages++ === n) {
      bench.end(n);
      port1.close();
    } else {
      send();
    }
  }

  function send() {
    const stream = createPayload();

    port1.postMessage(stream, [stream]);
  }

  bench.start();
  send();
}
