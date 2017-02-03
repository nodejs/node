'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  type: ['raw', 'diff']
});

function main(conf) {
  const n = conf.n | 0;
  const hrtime = process.hrtime;
  var noDead = hrtime();
  var i;

  if (conf.type === 'raw') {
    bench.start();
    for (i = 0; i < n; i++) {
      noDead = hrtime();
    }
    bench.end(n);
  } else {
    bench.start();
    for (i = 0; i < n; i++) {
      noDead = hrtime(noDead);
    }
    bench.end(n);
  }

  assert.ok(Array.isArray(noDead));
}
