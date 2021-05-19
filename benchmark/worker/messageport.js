'use strict';

const common = require('../common.js');
const { MessageChannel } = require('worker_threads');
const bench = common.createBenchmark(main, {
  payload: ['string', 'object'],
  style: ['eventtarget', 'eventemitter'],
  n: [1e6],
});

function main(conf) {
  const n = conf.n;
  let payload;

  switch (conf.payload) {
    case 'string':
      payload = 'hello world!';
      break;
    case 'object':
      payload = { action: 'pewpewpew', powerLevel: 9001 };
      break;
    default:
      throw new Error('Unsupported payload type');
  }

  const { port1, port2 } = new MessageChannel();

  let messages = 0;
  function listener() {
    if (messages++ === n) {
      bench.end(n);
      port1.close();
    } else {
      write();
    }
  }

  switch (conf.style) {
    case 'eventtarget':
      port2.onmessage = listener;
      break;
    case 'eventemitter':
      port2.on('message', listener);
      break;
    default:
      throw new Error('Unsupported listener type');
  }

  bench.start();
  write();

  function write() {
    port1.postMessage(payload);
  }
}
