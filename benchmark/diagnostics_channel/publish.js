'use strict';
const common = require('../common.js');
const dc = require('node:diagnostics_channel');

const bench = common.createBenchmark(main, {
  n: [1e8],
  checkSubscribers: [1, 0],
  objSize: [10, 1e2, 1e3],
  subscribers: [0, 1, 10],
});

function noop() {}

function createObj(size) {
  return Array.from({ length: size }, (n) => ({
    foo: 'yarp',
    nope: {
      bar: '123',
      a: [1, 2, 3],
      baz: n,
      c: {},
      b: [],
    },
  }));
}

function main({ n, subscribers, checkSubscribers, objSize }) {
  const channel = dc.channel('test');
  for (let i = 0; i < subscribers; i++) {
    channel.subscribe(noop);
  }

  const data = createObj(objSize);

  bench.start();
  for (let i = 0; i < n; i++) {
    if (checkSubscribers) {
      if(channel.hasSubscribers) {
        channel.publish(data);
      }
    } else {
      channel.publish(data);
    }
  }
  bench.end(n);
}
