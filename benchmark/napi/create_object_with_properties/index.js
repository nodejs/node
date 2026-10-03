'use strict';

const assert = require('assert');
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
  operation: ['create', 'read'],
});

function main({ n, method, operation }) {
  if (operation === 'read') {
    read(n, method);
  } else if (method === 'new') {
    binding.createObjectWithPropertiesNew(n, bench, bench.start, bench.end);
  } else {
    binding.createObjectWithPropertiesOld(n, bench, bench.start, bench.end);
  }
}

// Reads three properties of one of 1000 objects, n times.
function read(n, method) {
  const objects = binding.createObjects(method === 'new', 1000);
  let length = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    const object = objects[i % objects.length];
    length += object.foo0.length + object.foo9.length + object.foo19.length;
  }
  bench.end(n);
  assert.ok(length > 0);
}
