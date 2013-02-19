var common = require('../common.js');
var PORT = common.PORT;

var bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  type: ['bytes', 'buffer'],
  length: [4, 1024, 102400],
  c: [50, 500]
});

function main(conf) {
  process.env.PORT = PORT;
  var spawn = require('child_process').spawn;
  var simple = require('path').resolve(__dirname, '../http_simple.js');
  var server = spawn(process.execPath, [simple]);
  setTimeout(function() {
    var path = '/' + conf.type + '/' + conf.length; //+ '/' + conf.chunks;
    var args = ['-r', 5000, '-t', 8, '-c', conf.c];

    bench.http(path, args, function() {
      server.kill();
    });
  }, 2000);
}
