'use strict';

const vm = require('vm');
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  stage: ['all', 'instantiate', 'evaluate'],
  n: [1000],
}, {
  flags: ['--experimental-vm-modules'],
});

function main({ stage, n }) {
  const arr = [new vm.SourceTextModule(`
    export const value = 42;
  `)];

  if (stage === 'all') {
    bench.start();
  }

  for (let i = 0; i < n; i++) {
    const m = new vm.SourceTextModule(`
      export { value } from 'mod${i}';
    `);
    arr.push(m);
    m.linkRequests([arr[i]]);
  }

  if (stage === 'instantiate') {
    bench.start();
  }
  arr[n].instantiate();
  if (stage === 'instantiate') {
    bench.end(n);
  }

  if (stage === 'evaluate') {
    bench.start();
  }
  arr[n].evaluate();
  if (stage === 'evaluate' || stage === 'all') {
    bench.end(n);
  }

  assert.strictEqual(arr[n].namespace.value, 42);
}
