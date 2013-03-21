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

  var msg = '"' + Array(len).join('.') + '"';
  var options = { 'stdio': ['ignore', 'ipc', 'ignore'] };
  var child = spawn('yes', [msg], options);

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
