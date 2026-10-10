'use strict';

const common = require('../../common.js');

// The addon calls into JS from a libuv timer, so every call opens a top-level
// callback scope, like an I/O callback does. type=Call is a plain
// v8::Function::Call, what the call costs without Node.

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error('napi/make_callback/index.js Binding failed to load');
  process.exit(0);
}

const types = ['MakeCallback', 'AsyncResource', 'Call'];

const bench = common.createBenchmark(main, {
  type: types,
  n: [1e6, 1e7],
});

function main({ type, n }) {
  bench.start();
  binding.run(n, () => {}, () => bench.end(n), types.indexOf(type));
}
