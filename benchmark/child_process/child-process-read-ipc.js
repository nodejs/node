'use strict';
if (process.argv[2] === 'child')
{
  var len = +process.argv[3];
  var msg = '"' + Array(len).join('.') + '"';
  while(true) {
    process.send(msg);
  }
} else {
  var common = require('../common.js');
  var bench = common.createBenchmark(main, {
    len: [64, 256, 1024, 4096, 32768],
    dur: [5]
  });
  var spawn = require('child_process').spawn;
  function main(conf) {
    bench.start();

    var dur = +conf.dur;
    var len = +conf.len;

    var options = { 'stdio': ['ignore', 'ignore', 'ignore', 'ipc'] };
    var child = spawn(process.argv[0],
      [process.argv[1], 'child', len], options);

    var bytes = 0;
    child.on('message', function(msg) {
      bytes += msg.length;
    });

    setTimeout(function() {
      child.kill();
      var gbits = (bytes * 8) / (1024 * 1024 * 1024);
      bench.end(gbits);
    }, dur * 1000);
  }
}
