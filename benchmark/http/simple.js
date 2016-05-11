'use strict';
var common = require('../common.js');
var PORT = common.PORT;

var bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  type: ['bytes', 'buffer'],
  length: [4, 1024, 102400],
  chunks: [0, 1, 4],  // chunks=0 means 'no chunked encoding'.
  c: [50, 500]
});

function main(conf) {
  process.env.PORT = PORT;
  var server = require('../http_simple.js');
  setTimeout(function() {
    var path = '/' + conf.type + '/' + conf.length + '/' + conf.chunks;
    var args = ['-t', '10s', '-c', conf.c, '-k'];

    bench.http(path, args, function() {
      server.close();
    });
  }, 2000);
}
