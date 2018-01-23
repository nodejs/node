'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  type: ['bytes', 'buffer'],
  len: [4, 1024, 102400],
  chunks: [1, 4],
  c: [50, 500],
  chunkedEnc: [1, 0],
  res: ['normal', 'setHeader', 'setHeaderWH']
});

function main({ type, len, chunks, c, chunkedEnc, res }) {
  var server = require('../fixtures/simple-http-server.js')
  .listen(common.PORT)
  .on('listening', function() {
    const path = `/${type}/${len}/${chunks}/${res}/${chunkedEnc}`;

    bench.http({
      path: path,
      connections: c
    }, function() {
      server.close();
    });
  });
}
