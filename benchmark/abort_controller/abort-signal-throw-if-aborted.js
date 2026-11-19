'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e7],
  kind: ['controller', 'timeout', 'any', 'mixed'],
});

const kSignals = 16;

function createSignal(kind, i, controllers) {
  switch (kind) {
    case 'controller': {
      const controller = new AbortController();
      controllers.push(controller);
      return controller.signal;
    }
    case 'timeout':
      return AbortSignal.timeout(1e9);
    case 'any': {
      const controller = new AbortController();
      controllers.push(controller);
      return AbortSignal.any([controller.signal, AbortSignal.timeout(1e9)]);
    }
    case 'mixed':
      return createSignal(['controller', 'timeout', 'any'][i % 3], i, controllers);
    default:
      throw new Error('Invalid kind');
  }
}

function main({ n, kind }) {
  // Keep the controllers alive so that composite signals keep their sources.
  const controllers = [];
  const signals = [];
  for (let i = 0; i < kSignals; ++i)
    signals.push(createSignal(kind, i, controllers));

  bench.start();
  for (let i = 0; i < n; ++i)
    signals[i % kSignals].throwIfAborted();
  bench.end(n);
}
