'use strict';
if (process.argv[2] === 'child') {
  const len = +process.argv[3];
  const msg = `"${'.'.repeat(len)}"`;
  while (true) {
    process.send(msg);
  }
} else {
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

    const options = { 'stdio': ['ignore', 'ignore', 'ignore', 'ipc'] };
    const child = spawn(process.argv[0],
      [process.argv[1], 'child', len], options);

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
}
