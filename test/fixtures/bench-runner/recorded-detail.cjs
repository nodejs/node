'use strict';

const { bench } = require('node:bench');

bench('recorded detail', { samples: 3 }, (b) => {
  b.record({
    __proto__: null,
    detail: {
      index: b.index,
      phase: b.phase,
      value: 42n,
    },
    duration_ns: 4n,
    operations: 2,
  });
  b.done();
});
