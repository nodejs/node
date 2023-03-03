'use strict';
const common = require('../common.js');
const { URL } = require('url');
const fs = require('fs');
const path = require('path');

// This benchmark uses 100,000 unique URLs gathered by the top 100 visited
// websites in the world. It is used to understand the real world impact of
// the URL parser performance.
// Reference: https://github.com/ada-url/url-dataset
const filePath = path.join(__dirname, '../../test/fixtures/url-dataset.txt');
const lines = fs.readFileSync(filePath, 'utf8').split('\n');

const bench = common.createBenchmark(main, {
  n: [1, 5, 10],
});

function main({ n }) {
  let result = 0;

  bench.start();
  for (let i = 0; i < n; i++) {
    for (const line of lines) {
      try {
        const url = new URL(line);
        // eslint-disable-next-line no-unused-vars
        result += url.toString().length;
      } catch {
        // do nothing
      }
    }
  }
  bench.end(n);
}
