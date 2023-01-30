'use strict';
const common = require('../common.js');

const inputs = {
  valid: 'adsfadsfadsf',
  validsurr: '\uda23\ude23\uda1f\udfaa\ud800\udfff\uda23\ude23\uda1f\udfaa' +
             '\ud800\udfff',
  someinvalid: 'asasfdfasd\uda23',
  allinvalid: '\udc45\uda23 \udf00\udc00 \udfaa\uda12 \udc00\udfaa',
  nonstring: { toString() { return 'asdf'; } },
};
const bench = common.createBenchmark(main, {
  input: Object.keys(inputs),
  n: [5e7],
}, {
  flags: ['--expose-internals'],
});

function main({ input, n }) {
  const { toUSVString } = require('internal/url');
  const str = inputs[input];

  bench.start();
  for (let i = 0; i < n; i++)
    toUSVString(str);
  bench.end(n);
}
