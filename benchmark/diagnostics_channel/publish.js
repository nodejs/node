'use strict';
const common = require('../common.js');
const dc = require('diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e8],
  subscribers: [0, 1, 10],
});

function noop() {}

function main({ n, subscribers }) {
  const channel = dc.channel('test');
  for (let i = 0; i < subscribers; i++) {
    channel.subscribe(noop);
  }

  const data = {
    foo: 'bar',
  };

  bench.start();
  for (let i = 0; i < n; i++) {
    if (channel.hasSubscribers) {
      channel.publish(data);
    }
  }
  bench.end(n);
}
