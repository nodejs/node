'use strict';

const common = require('../../common.js');

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error(`${__filename}: Binding failed to load`);
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  n: [1e2, 1e3, 1e4, 1e5, 1e6],
  method: ['new', 'old'],
});

function main({ n, method }) {
  if (method === 'new') {
    binding.createObjectWithPropertiesNew(n, bench, bench.start, bench.end);
  } else {
    binding.createObjectWithPropertiesOld(n, bench, bench.start, bench.end);
  }
}
