'use strict';
const common = require('../common.js');
const { OutgoingMessage } = require('_http_outgoing');
const { Writable } = require('stream');

const bench = common.createBenchmark(main, {
  type: ['string', 'buffer', 'mixed'],
  n: [100000, 500000],
  arraySize: [1, 5, 10],
});

function main({ n, type, arraySize }) {
  // Create a mock socket
  const socket = new Writable({
    write(chunk, encoding, callback) {
      callback();
    },
  });

  const message = new OutgoingMessage({ _headers: {} });
  message.socket = socket;
  message._contentLength = 100; // Trigger content-disposition encoding

  let value;
  if (arraySize === 1) {
    if (type === 'string') {
      value = 'attachment; filename="test.txt"';
    } else if (type === 'buffer') {
      value = Buffer.from('attachment; filename="test.txt"', 'latin1');
    } else {
      value = Math.random() > 0.5 ?
        'attachment; filename="test.txt"' :
        Buffer.from('attachment; filename="test.txt"', 'latin1');
    }
  } else {
    value = [];
    for (let i = 0; i < arraySize; i++) {
      if (type === 'string') {
        value.push(`attachment; filename="test${i}.txt"`);
      } else if (type === 'buffer') {
        value.push(Buffer.from(`attachment; filename="test${i}.txt"`, 'latin1'));
      } else {
        value.push(Math.random() > 0.5 ?
          `attachment; filename="test${i}.txt"` :
          Buffer.from(`attachment; filename="test${i}.txt"`, 'latin1'));
      }
    }
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    message.setHeader('content-disposition', value);
    message.removeHeader('content-disposition');
  }
  bench.end(n);
}