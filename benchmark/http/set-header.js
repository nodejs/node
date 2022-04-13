'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  res: ['normal', 'setHeader', 'setHeaderWH'],
  duration: 5
});

const type = 'bytes';
const len = 4;
const chunks = 0;
const chunkedEnc = 0;
const c = 50;

// normal: writeHead(status, {...})
// setHeader: statusCode = status, setHeader(...) x2
// setHeaderWH: setHeader(...), writeHead(status, ...)
function main({ res, duration }) {
  const server = require('../fixtures/simple-http-server.js')
  .listen(0)
  .on('listening', () => {
    const path = `/${type}/${len}/${chunks}/${res}/${chunkedEnc}`;

    bench.http({
      path: path,
      connections: c,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
