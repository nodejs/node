'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  len: [64, 256, 1024, 4096, 32768],
  dur: [5]
});

const spawn = require('child_process').spawn;
function main(conf) {
  bench.start();

  const dur = +conf.dur;
  const len = +conf.len;

  const msg = '"' + Array(len).join('.') + '"';
  const options = {'stdio': ['ignore', 'ipc', 'ignore']};
  const child = spawn('yes', [msg], options);

  var bytes = 0;
  child.on('message', function(msg) {
    bytes += msg.length;
  });

  setTimeout(function() {
    child.kill();
    const gbits = (bytes * 8) / (1024 * 1024 * 1024);
    bench.end(gbits);
  }, dur * 1000);
}
