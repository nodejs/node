'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  n: [1, 100, 10_000],
  dereference: ['true', 'false'],
  force: ['true', 'false'],
});

function prepareTestDirectory() {
  const testDir = tmpdir.resolve(`test-cp-${process.pid}`);
  fs.mkdirSync(testDir, { recursive: true });

  fs.writeFileSync(path.join(testDir, 'source.txt'), 'test content');

  fs.symlinkSync(
    path.join(testDir, 'source.txt'),
    path.join(testDir, 'link.txt'),
  );

  return testDir;
}

function main({ n, dereference, force }) {
  tmpdir.refresh();

  const src = prepareTestDirectory();
  const dest = tmpdir.resolve(`${process.pid}/subdir/cp-bench-${process.pid}`);

  const options = {
    recursive: true,
    dereference: dereference === 'true',
    force: force === 'true',
  };

  if (options.force) {
    fs.cpSync(src, dest, { recursive: true });
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    if (options.force) {
      fs.cpSync(src, dest, options);
    } else {
      const uniqueDest = tmpdir.resolve(
        `${process.pid}/subdir/cp-bench-${process.pid}-${i}`,
      );
      fs.cpSync(src, uniqueDest, options);
    }
  }
  bench.end(n);
}
