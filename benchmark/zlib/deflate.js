'use strict';
var common = require('../common.js');
var zlib = require('zlib');

var bench = common.createBenchmark(main, {
  method: ['createDeflate', 'deflate', 'deflateSync'],
  inputLen: [1024],
  n: [4e5]
});

function main(conf) {
  var n = +conf.n;
  var method = conf.method;
  var chunk = Buffer.alloc(+conf.inputLen, 'a');

  var i = 0;
  switch (method) {
    // Performs `n` writes for a single deflate stream
    case 'createDeflate':
      var deflater = zlib.createDeflate();
      deflater.resume();
      deflater.on('finish', () => {
        bench.end(n);
      });

      bench.start();
      (function next() {
        if (i++ === n)
          return deflater.end();
        deflater.write(chunk, next);
      })();
      break;
    // Performs `n` single deflate operations
    case 'deflate':
      var deflate = zlib.deflate;
      bench.start();
      (function next(err, result) {
        if (i++ === n)
          return bench.end(n);
        deflate(chunk, next);
      })();
      break;
    // Performs `n` single deflateSync operations
    case 'deflateSync':
      var deflateSync = zlib.deflateSync;
      bench.start();
      for (; i < n; ++i)
        deflateSync(chunk);
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported deflate method');
  }
}
