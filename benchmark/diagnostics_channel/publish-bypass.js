'use strict';
const common = require('../common.js');
const dc = require('node:diagnostics_channel');

// This benchmark requires a binary supporting bypassId and cannot run on main.

const bench = common.createBenchmark(main, {
  mode: ['normal', 'bypass-idle', 'bypass-active'],
  n: [1e7],
  subscribers: [1, 10],
});

function noop() {}

function main({ mode, n, subscribers }) {
  const channel = dc.channel('test');
  const key = Symbol('key');
  const data = {};

  for (let i = 0; i < subscribers; i++) {
    if (mode === 'normal') {
      channel.subscribe(noop);
    } else {
      channel.subscribe(noop, { bypassId: key });
    }
  }

  const publish = () => {
    bench.start();
    for (let i = 0; i < n; i++) {
      channel.publish(data);
    }
    bench.end(n);
  };

  switch (mode) {
    case 'normal':
    case 'bypass-idle':
      publish();
      break;
    case 'bypass-active':
      dc.bypass(key, publish);
      break;
    default:
      throw new Error(`Unsupported mode: ${mode}`);
  }
}
