'use strict';

const common = require('../common');
const util = require('util');

const bench = common.createBenchmark(main, {
  n: [1e3],
  len: [1e2, 1e4],
  maxObjectProperties: [0, 10, 100, Infinity],
  showHidden: [0, 1],
});

function main({ n, len, maxObjectProperties, showHidden }) {
  const prototype = {};
  const object = { __proto__: prototype };
  for (let i = 0; i < len; i++) {
    object[`property${i}`] = { value: i };
    prototype[`prototypeProperty${i}`] = { value: i };
  }

  const options = {
    maxObjectProperties,
    showHidden: showHidden === 1,
  };

  bench.start();
  for (let i = 0; i < n; i++) {
    util.inspect(object, options);
  }
  bench.end(n);
}
