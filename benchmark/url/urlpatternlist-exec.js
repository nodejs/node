'use strict';
const common = require('../common.js');
const { URLPattern, URLPatternList } = require('url');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  routes: [10, 100, 1000],
  input: ['first', 'last', 'none'],
  method: ['URLPatternList', 'URLPattern'],
  n: [1e4],
}, {
  flags: ['--disable-warning=ExperimentalWarning'],
});

function main({ routes, input, method, n }) {
  const patterns = [];
  for (let i = 0; i < routes; i++) {
    patterns.push(new URLPattern({ pathname: `/api/resource${i}/:id` }));
  }
  const url = {
    first: 'https://example.com/api/resource0/42',
    last: `https://example.com/api/resource${routes - 1}/42`,
    none: 'https://example.com/api/missing/42',
  }[input];

  let exec;
  if (method === 'URLPatternList') {
    const list = new URLPatternList(patterns);
    exec = (url) => list.exec(url);
  } else {
    // The loop that URLPatternList#exec() is specified as.
    exec = (url) => {
      for (const pattern of patterns) {
        const result = pattern.exec(url);
        if (result !== null) return result;
      }
      return null;
    };
  }

  let result;
  bench.start();
  for (let i = 0; i < n; i++) {
    result = exec(url);
  }
  bench.end(n);
  assert.strictEqual(result === null, input === 'none');
}
