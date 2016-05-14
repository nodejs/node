'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  len: [64, 256, 1024, 4096, 32768],
  dur: [5]
});

const exec = require('child_process').exec;
function main(conf) {
  bench.start();

  const dur = +conf.dur;
  const len = +conf.len;

  const msg = `"${'.'.repeat(len)}"`;
  msg.match(/./);
  const options = {'stdio': ['ignore', 'pipe', 'ignore']};
  // NOTE: Command below assumes bash shell.
  const child = exec(`while\n  echo ${msg}\ndo :; done\n`, options);

  var bytes = 0;
  child.stdout.on('data', function(msg) {
    bytes += msg.length;
  });

  setTimeout(function() {
    child.kill();
    bench.end(bytes);
  }, dur * 1000);
}
