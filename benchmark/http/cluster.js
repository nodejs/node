'use strict';
const common = require('../common.js');
const PORT = common.PORT;

const cluster = require('cluster');
let bench;
if (cluster.isMaster) {
  bench = common.createBenchmark(main, {
    // Unicode confuses ab on os x.
    type: ['bytes', 'buffer'],
    len: [4, 1024, 102400],
    c: [50, 500],
    duration: 5,
  });
} else {
  const port = parseInt(process.env.PORT || PORT);
  require('../fixtures/simple-http-server.js').listen(port);
}

function main({ type, len, c, duration }) {
  process.env.PORT = PORT;
  let workers = 0;
  const w1 = cluster.fork();
  const w2 = cluster.fork();

  cluster.on('listening', () => {
    workers++;
    if (workers < 2)
      return;

    setImmediate(() => {
      const path = `/${type}/${len}`;

      bench.http({
        path: path,
        connections: c,
        duration
      }, () => {
        w1.destroy();
        w2.destroy();
      });
    });
  });
}
