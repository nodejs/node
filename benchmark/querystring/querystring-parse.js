'use strict';
const common = require('../common.js');
const querystring = require('querystring');
const inputs = common.searchParams;

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e6],
});

function main({ type, n }) {
  const input = inputs[type];

  // Execute the function a "sufficient" number of times before the timed
  // loop to ensure the function is optimized just once.
  if (type === 'multicharsep') {
    for (let i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');

    bench.start();
    for (let i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
    bench.end(n);
  } else {
    for (let i = 0; i < n; i += 1)
      querystring.parse(input);

    bench.start();
    for (let i = 0; i < n; i += 1)
      querystring.parse(input);
    bench.end(n);
  }
}
