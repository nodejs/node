'use strict';

const common = require('../common.js');
const { OutgoingMessage } = require('_http_outgoing');

const bench = common.createBenchmark(main, {
  value: [
    'X-Powered-By',
    'Vary',
    'Set-Cookie',
    'Content-Type',
    'Content-Length',
    'Connection',
    'Transfer-Encoding',
  ],
  n: [1e6],
});

function main({ n, value }) {
  const og = new OutgoingMessage();

  bench.start();
  for (let i = 0; i < n; i++) {
    og.setHeader(value, '');
  }
  bench.end(n);
}
