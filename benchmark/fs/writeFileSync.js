'use strict';

const path = require('path');
const common = require('../common.js');
const filename = path.resolve(__dirname, '.removeme-benchmark-garbage');
const fs = require('fs');
const writeFileSync = fs.writeFileSync;

const bench = common.createBenchmark(main, {
  n: [1e5],
  file: [''],
  type: ['buf', 'asc', 'utf'],
  size: [2, 1024, 64 * 1024, 1024 * 1024]
});

function main(conf) {
  const n = +conf.n;
  const size = +conf.size;
  const file = (conf.file.length === 0 ? filename : conf.file);
  var encoding;
  var data;
  var i;

  try { fs.unlinkSync(file); } catch (e) {}
  process.on('exit', function() {
    try { fs.unlinkSync(file); } catch (e) {}
  });

  switch (conf.type) {
    case 'buf':
      data = Buffer.alloc(size, 'b');
      bench.start();
      for (i = 0; i < n; ++i)
        writeFileSync(file, data);
      bench.end(n);
      return;
    case 'asc':
      data = new Array(size + 1).join('a');
      encoding = 'ascii';
      break;
    case 'utf':
      data = new Array(Math.ceil(size / 2) + 1).join('ü');
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  bench.start();
  for (i = 0; i < n; ++i)
    writeFileSync(file, data, encoding);
  bench.end(n);
}
