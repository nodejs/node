'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: ['raw', 'diff', 'bigint']
});

function main({ n, type }) {
  const hrtime = process.hrtime;
  var noDead = type === 'bigint' ? hrtime.bigint() : hrtime();
  var i;

  switch (type) {
    case 'raw':
      bench.start();
      for (i = 0; i < n; i++) {
        noDead = hrtime();
      }
      bench.end(n);
      break;
    case 'diff':
      bench.start();
      for (i = 0; i < n; i++) {
        noDead = hrtime(noDead);
      }
      bench.end(n);
      break;
    case 'bigint':
      bench.start();
      for (i = 0; i < n; i++) {
        noDead = hrtime.bigint();
      }
      bench.end(n);
      break;
  }

  // eslint-disable-next-line valid-typeof
  assert.ok(Array.isArray(noDead) || typeof noDead === 'bigint');
}
