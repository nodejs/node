'use strict';

// fs.promises.cp() of a directory tree.

const common = require('../common');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  files: [500],
  n: [3],
});

function prepareSource(files) {
  const src = tmpdir.resolve('cp-src');
  for (let i = 0; i < files; i++) {
    const dir = path.join(src, `dir-${i % 10}`, `sub-${i % 7}`);
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, `file-${i}.js`), 'x'.repeat(1024 + (i % 512)));
  }
  return src;
}

async function main({ files, n }) {
  tmpdir.refresh();
  const src = prepareSource(files);
  bench.start();
  for (let i = 0; i < n; i++) {
    await fs.promises.cp(src, tmpdir.resolve(`cp-dest-${i}`), { recursive: true });
  }
  bench.end(n);
}
