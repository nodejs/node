'use strict';
const common = require('../common.js');
const assert = require('assert');
const tmpdir = require('../../test/common/tmpdir');
const fixtures = require('../../test/common/fixtures.js');
const fs = require('fs');

// This fixture has 2000+ exports.
const fixtureFile = fixtures.path('snapshot', 'typescript.js');
// This is fixed - typescript.js is slow to load, so 10 is enough.
// And we want to measure either 10 warm loads, or 1 cold load (but compare.js would run it many times)
const runs = 10;
const bench = common.createBenchmark(main, {
  type: ['cold', 'warm'],
}, {
  setup() {
    tmpdir.refresh();
    fs.cpSync(fixtureFile, tmpdir.resolve(`imported-cjs-initial.js`));
    for (let i = 0; i < runs; i++) {
      fs.cpSync(fixtureFile, tmpdir.resolve(`imported-cjs-${i}.js`));
    }
  },
});

async function main({ type }) {
  if (type === 'cold') {
    bench.start();
    await import(tmpdir.fileURL(`imported-cjs-initial.js`));
    bench.end(1);
  } else {
    await import(tmpdir.fileURL(`imported-cjs-initial.js`));  // Initialize the wasm first.
    bench.start();
    let result;
    for (let i = 0; i < runs; i++) {
      result = await import(tmpdir.fileURL(`imported-cjs-${i}.js`));
    }
    bench.end(runs);
    const mod = require(fixtures.path('snapshot', 'typescript.js'));
    assert.deepStrictEqual(Object.keys(mod), Object.keys(result.default));
  }
}
