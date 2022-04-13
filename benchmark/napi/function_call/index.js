// Show the difference between calling a short js function
// relative to a comparable C++ function.
// Reports n of calls per second.
// Note that JS speed goes up, while cxx speed stays about the same.
'use strict';

const assert = require('assert');
const common = require('../../common.js');

// This fails when we try to open with a different version of node,
// which is quite common for benchmarks.  so in that case, just
// abort quietly.

let binding;
try {
  binding = require(`./build/${common.buildType}/binding`);
} catch {
  console.error('misc/function_call.js Binding failed to load');
  process.exit(0);
}
const cxx = binding.hello;

let napi_binding;
try {
  napi_binding = require(`./build/${common.buildType}/napi_binding`);
} catch {
  console.error('misc/function_call/index.js NAPI-Binding failed to load');
  process.exit(0);
}
const napi = napi_binding.hello;

let c = 0;
function js() {
  return c++;
}

assert(js() === cxx());

const bench = common.createBenchmark(main, {
  type: ['js', 'cxx', 'napi'],
  n: [1e6, 1e7, 5e7]
});

function main({ n, type }) {
  const fn = type === 'cxx' ? cxx : type === 'napi' ? napi : js;
  bench.start();
  for (let i = 0; i < n; i++) {
    fn();
  }
  bench.end(n);
}
