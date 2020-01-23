'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  encoding: [
    '', 'utf8', 'ascii', 'hex', 'utf16le', 'latin1',
  ],
  args: [ '', 'offset', 'offset+length' ],
  len: [2048],
  n: [1e6]
});

function main({ len, n, encoding, args }) {
  let string;
  let start = 0;
  const buf = Buffer.allocUnsafe(len);

  switch (args) {
    case 'offset':
      string = 'a'.repeat(Math.floor(len / 2));
      start = len - string.length;
      if (encoding) {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string, start, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string, start);
        }
        bench.end(n);
      }
      break;
    case 'offset+length':
      string = 'a'.repeat(len);
      if (encoding) {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string, 0, buf.length, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string, 0, buf.length);
        }
        bench.end(n);
      }
      break;
    default:
      string = 'a'.repeat(len);
      if (encoding) {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string, encoding);
        }
        bench.end(n);
      } else {
        bench.start();
        for (let i = 0; i < n; ++i) {
          buf.write(string);
        }
        bench.end(n);
      }
  }
}
