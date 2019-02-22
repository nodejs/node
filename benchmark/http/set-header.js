'use strict';
const common = require('../common.js');
const PORT = common.PORT;

const bench = common.createBenchmark(main, {
  res: ['normal', 'setHeader', 'setHeaderWH']
});

const type = 'bytes';
const len = 4;
const chunks = 0;
const chunkedEnc = 0;
const c = 50;

// normal: writeHead(status, {...})
// setHeader: statusCode = status, setHeader(...) x2
// setHeaderWH: setHeader(...), writeHead(status, ...)
function main({ res }) {
  process.env.PORT = PORT;
  var server = require('../fixtures/simple-http-server.js')
  .listen(PORT)
  .on('listening', () => {
    const path = `/${type}/${len}/${chunks}/normal/${chunkedEnc}`;

    bench.http({
      path: path,
      connections: c
    }, () => {
      server.close();
    });
  });
}
