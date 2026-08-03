'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');
const { strictEqual } = require('assert');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = tmpdir.resolve('benchmark-esm-parse');

const bench = common.createBenchmark(main, {
  n: [1e2],
});

async function main({ n }) {
  tmpdir.refresh();

  fs.mkdirSync(benchmarkDirectory);

  let sampleSource = 'try {\n';
  for (let i = 0; i < 1000; i++) {
    sampleSource += 'sample.js(() => file = /test/);\n';
  }
  sampleSource += '} catch {}\nexports.p = 5;\n';

  for (let i = 0; i < n; i++) {
    const sampleFile = path.join(benchmarkDirectory, `sample${i}.js`);
    fs.writeFileSync(sampleFile, sampleSource);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const sampleFile = path.join(benchmarkDirectory, `sample${i}.js`);
    const m = await import('file:' + sampleFile);
    strictEqual(m.p, 5);
  }
  bench.end(n);

  tmpdir.refresh();
}
