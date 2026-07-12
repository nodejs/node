'use strict';

const vm = require('vm');
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  stage: ['all', 'link', 'instantiate', 'evaluate'],
  n: [1000],
}, {
  flags: ['--experimental-vm-modules'],
});

function main({ stage, n }) {
  const arr = [];
  let importSource = '';
  let useSource = 'export const result = 0 ';
  for (let i = 0; i < n; i++) {
    importSource += `import { value${i} } from 'mod${i}';\n`;
    useSource += ` + value${i}\n`;
  }

  if (stage === 'all') {
    bench.start();
  }
  for (let i = 0; i < n; i++) {
    const m = new vm.SourceTextModule(`
      export const value${i} = 1;
    `);
    arr.push(m);
  }

  const root = new vm.SourceTextModule(`
    ${importSource}
    ${useSource};
  `);

  if (stage === 'link') {
    bench.start();
  }

  root.linkRequests(arr);
  for (let i = 0; i < n; i++) {
    arr[i].linkRequests([]);
  }

  if (stage === 'link') {
    bench.end(n);
  }

  if (stage === 'instantiate') {
    bench.start();
  }
  root.instantiate();
  if (stage === 'instantiate') {
    bench.end(n);
  }

  if (stage === 'evaluate') {
    bench.start();
  }
  root.evaluate();
  if (stage === 'evaluate' || stage === 'all') {
    bench.end(n);
  }

  assert.strictEqual(root.namespace.result, n);
}
