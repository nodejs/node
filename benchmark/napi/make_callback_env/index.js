'use strict';

const common = require('../../common.js');

// node::MakeCallback from a libuv timer, the Environment looked up from the
// callback on every call (MakeCallback) or passed in (MakeCallbackEnv).

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error('napi/make_callback_env/index.js Binding failed to load');
  process.exit(0);
}

const types = ['MakeCallback', 'MakeCallbackEnv'];

const bench = common.createBenchmark(main, {
  type: types,
  n: [1e6, 1e7],
});

function main({ type, n }) {
  bench.start();
  binding.run(n, () => {}, () => bench.end(n), types.indexOf(type));
}
