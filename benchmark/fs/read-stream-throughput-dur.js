'use strict';

if (process.platform === 'win32')
  throw new Error('This benchmark is not compatible with Windows');

const common = require('../common.js');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  type: ['buf', 'asc', 'utf'],
  dur: [5],
  size: [1024, 4096, 65535, 1024 * 1024]
});

function main(conf) {
  const dur = +conf.dur * 1000;
  const hwm = +conf.size;
  var bytes = 0;
  var encoding;

  switch (conf.type) {
    case 'buf':
      encoding = null;
      break;
    case 'asc':
      encoding = 'ascii';
      break;
    case 'utf':
      encoding = 'utf8';
      break;
    default:
      throw new Error('invalid type');
  }

  setTimeout(() => {
    // MB/sec
    bench.end(bytes / (1024 * 1024));
    process.exit(0);
  }, dur);

  fs.createReadStream('/dev/zero', {
    highWaterMark: hwm,
    encoding: encoding
  }).on('open', function() {
    bench.start();
  }).on('data', function(chunk) {
    bytes += chunk.length;
  });
}
