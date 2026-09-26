'use strict';

const common = require('../../common.js');

// The addon calls into JS with node::MakeCallback from a libuv timer, so
// every call opens a top-level callback scope, like an I/O callback does.

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error('napi/make_callback/index.js Binding failed to load');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  n: [1e6, 1e7],
});

function main({ n }) {
  bench.start();
  binding.run(n, () => {}, () => bench.end(n));
}
