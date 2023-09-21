'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const nonexistentDir = tmpdir.resolve('non-existent', 'foo', 'bar');
const bench = common.createBenchmark(main, {
  type: ['valid', 'invalid'],
  n: [1e3],
});

function main({ n, type }) {
  let prefix;

  switch (type) {
    case 'valid':
      prefix = `${Date.now()}`;
      break;
    case 'invalid':
      prefix = nonexistentDir;
      break;
    default:
      new Error('Invalid type');
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      const folderPath = fs.mkdtempSync(prefix, { encoding: 'utf8' });
      fs.rmdirSync(folderPath);
    } catch {
      // do nothing
    }
  }
  bench.end(n);
}
