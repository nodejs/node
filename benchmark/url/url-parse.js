'use strict';
const common = require('../common.js');
const url = require('url');

const inputs = {
  ipV6: 'http://[::1]:3000',
  ipV4: 'http://127.0.0.1:3000',
  withPort: 'http://foo.bar:3000',
  normal: 'http://foo.com/bar',
  escaped: 'https://foo.bar/{}^`/abcd',
};

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e7],
});

function main({ type, n }) {
  const input = inputs[type];

  bench.start();
  for (let i = 0; i < n; i += 1)
    url.parse(input);
  bench.end(n);
}
