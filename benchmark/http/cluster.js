var common = require('../common.js');
var PORT = common.PORT;

var cluster = require('cluster');
if (cluster.isMaster) {
  var bench = common.createBenchmark(main, {
    // unicode confuses ab on os x.
    type: ['bytes', 'buffer'],
    length: [4, 1024, 102400],
    c: [50, 150]
  });
} else {
  require('../http_simple.js');
}

function main(conf) {
  process.env.PORT = PORT;
  var workers = 0;
  var w1 = cluster.fork();
  var w2 = cluster.fork();

  cluster.on('listening', function() {
    workers++;
    if (workers < 2)
      return;

    setTimeout(function() {
      var path = '/' + conf.type + '/' + conf.length;
      var args = ['-r', '-t', 5, '-c', conf.c, '-k'];

      bench.ab(path, args, function() {
        w1.destroy();
        w2.destroy();
      });
    }, 2000);
  });
}
