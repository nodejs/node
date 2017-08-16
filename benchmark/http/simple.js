'use strict';
var common = require('../common.js');
var PORT = common.PORT;

var bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  type: ['bytes', 'buffer'],
  len: [4, 1024, 102400],
  chunks: [1, 4],
  c: [50, 500],
  chunkedEnc: ['true', 'false'],
  res: ['normal', 'setHeader', 'setHeaderWH']
});

function main(conf) {
  process.env.PORT = PORT;
  var server = require('../fixtures/simple-http-server.js')
  .listen(process.env.PORT || common.PORT)
  .on('listening', function() {
    var path =
      `/${conf.type}/${conf.len}/${conf.chunks}/${conf.res}/${conf.chunkedEnc}`;

    bench.http({
      path: path,
      connections: conf.c
    }, function() {
      server.close();
    });
  });
}
