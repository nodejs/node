'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const path = tmpdir.resolve(`new-file-${process.pid}`);
const parameters = [Buffer.from('Benchmark data'),
                    0,
                    Buffer.byteLength('Benchmark data')];
const bench = common.createBenchmark(main, {
  type: ['valid', 'invalid'],
  n: [1e5],
});

function main({ n, type }) {
  let fd;
  let result;

  switch (type) {
    case 'valid':
      fd = fs.openSync(path, 'w');

      bench.start();
      for (let i = 0; i < n; i++) {
        result = fs.writeSync(fd, ...parameters);
      }

      bench.end(n);
      assert(result);
      fs.closeSync(fd);
      break;
    case 'invalid': {
      fd = 1 << 30;
      let hasError = false;
      bench.start();
      for (let i = 0; i < n; i++) {
        try {
          result = fs.writeSync(fd, ...parameters);
        } catch {
          hasError = true;
        }
      }

      bench.end(n);
      assert(hasError);
      break;
    }
    default:
      throw new Error('Invalid type');
  }
}
