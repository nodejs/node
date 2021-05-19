'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['bytes', 'buffer'],
  len: [4, 1024, 102400],
  chunks: [1, 4],
  c: [50, 500],
  chunkedEnc: [1, 0],
  benchmarker: ['test-double-https'],
  duration: 5,
});

function main({ type, len, chunks, c, chunkedEnc, duration }) {
  const server = require('../fixtures/simple-https-server.js')
  .listen(common.PORT)
  .on('listening', () => {
    const path = `/${type}/${len}/${chunks}/${chunkedEnc}`;

    bench.http({
      path,
      connections: c,
      scheme: 'https',
      duration,
    }, () => {
      server.close();
    });
  });
}
