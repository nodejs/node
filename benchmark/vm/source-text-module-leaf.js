'use strict';

const vm = require('vm');
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  stage: ['all', 'compile', 'link', 'instantiate', 'evaluate'],
  type: ['sync', 'async'],
  n: [1000],
}, {
  flags: ['--experimental-vm-modules'],
});

function main({ stage, type, n }) {
  const arr = [];
  if (stage === 'all' || stage === 'compile') {
    bench.start();
  }

  for (let i = 0; i < n; i++) {
    let source = `export const value${i} = 1;`;
    if (type === 'async') {
      source += `\nawait Promise.resolve();\n`;
    }
    const m = new vm.SourceTextModule(source);
    arr.push(m);
  }

  if (stage === 'compile') {
    bench.end(n);
    return;
  }

  if (stage === 'link') {
    bench.start();
  }

  for (let i = 0; i < n; i++) {
    arr[i].linkRequests([]);
  }

  if (stage === 'link') {
    bench.end(n);
    return;
  }

  if (stage === 'instantiate') {
    bench.start();
  }

  for (let i = 0; i < n; i++) {
    arr[i].instantiate();
  }

  if (stage === 'instantiate') {
    bench.end(n);
    return;
  }

  if (stage === 'evaluate') {
    bench.start();
  }

  function finalize() {
    bench.end(n);
    for (let i = 0; i < n; i++) {
      assert.strictEqual(arr[i].namespace[`value${i}`], 1);
    }
  }

  if (type === 'sync') {
    for (let i = 0; i < n; i++) {
      arr[i].evaluate();
    }
    finalize();
  } else {
    const promises = [];
    for (let i = 0; i < n; i++) {
      promises.push(arr[i].evaluate());
    }
    Promise.all(promises).then(finalize);
  }
}
