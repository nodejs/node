'use strict';
const path = require('path');
// Use an absolute path as the old loader was faster using an absolute path.
const commonPath = path.resolve(__dirname, '../common.js');
const common = require(commonPath);

const bench = common.createBenchmark(main, {
  n: [1000],
  useCache: ['true', 'false']
});

function main({ n, useCache }) {
  const natives = [
    'vm', 'util', 'child_process', 'dns', 'querystring', 'http', 'process',
    'http2', 'net', 'os', 'buffer', 'crypto', 'url', 'path', 'string_decoder'
  ];
  if (useCache) {
    for (const name of natives)
      require(name);
    require(commonPath);
  }

  bench.start();
  for (var i = 0; i < n; i++) {
    for (var j = 0; j < natives.length; j++)
      require(natives[j]);
    // Pretend mixed input (otherwise the results are less representative due to
    // highly specialized code).
    require(commonPath);
  }
  bench.end(n * (natives.length + 1));
}
