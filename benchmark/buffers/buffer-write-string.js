'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  encoding: [
    '', 'utf8', 'ascii', 'hex', 'UCS-2', 'utf16le', 'latin1', 'binary'
  ],
  args: [ '', 'offset', 'offset+length' ],
  len: [10, 2048],
  n: [1e7]
});

function main(conf) {
  const len = +conf.len;
  const n = +conf.n;
  const encoding = conf.encoding;
  const args = conf.args;

  const string = 'a'.repeat(len);
  const buf = Buffer.allocUnsafe(len);

  var i;

  switch (args) {
    case 'offset':
      if (encoding) {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string, 0, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string, 0);
        }
        bench.end(n);
      }
      break;
    case 'offset+length':
      if (encoding) {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string, 0, buf.length, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string, 0, buf.length);
        }
        bench.end(n);
      }
      break;
    default:
      if (encoding) {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (i = 0; i < n; ++i) {
          buf.write(string);
        }
        bench.end(n);
      }
  }
}
