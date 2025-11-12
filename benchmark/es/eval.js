'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['without-sourcemap', 'sourcemap'],
  n: [1e6],
});

const sourceWithoutSourceMap = `
'use strict';
(function() {
  let a = 1;
  for (let i = 0; i < 1000; i++) {
    a++;
  }
  return a;
})();
`;
const sourceWithSourceMap = `
${sourceWithoutSourceMap}
//# sourceMappingURL=https://ci.nodejs.org/405
`;

function evalN(n, source) {
  bench.start();
  for (let i = 0; i < n; i++) {
    eval(source);
  }
  bench.end(n);
}

function main({ n, method }) {
  switch (method) {
    case 'without-sourcemap':
      process.setSourceMapsEnabled(false);
      evalN(n, sourceWithoutSourceMap);
      break;
    case 'sourcemap':
      process.setSourceMapsEnabled(true);
      evalN(n, sourceWithSourceMap);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
