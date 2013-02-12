var common = require('../common.js');
var PORT = common.PORT;

var bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  type: ['bytes', 'buffer'],
  length: [4, 1024, 102400],
  c: [50, 150]
});

function main(conf) {
  process.env.PORT = PORT;
  var spawn = require('child_process').spawn;
  var simple = require('path').resolve(__dirname, '../http_simple.js');
  var server = spawn(process.execPath, [simple]);
  setTimeout(function() {
    var path = '/' + conf.type + '/' + conf.length; //+ '/' + conf.chunks;
    var args = ['-r', '-t', 5];

    if (+conf.c !== 1)
      args.push('-c', conf.c);

    args.push('-k');

    bench.ab(path, args, function() {
      server.kill();
    });
  }, 2000);
}
