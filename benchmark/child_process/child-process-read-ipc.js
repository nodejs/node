'use strict';
if (process.argv[2] === 'child') {
  const len = +process.argv[3];
  const msg = '.'.repeat(len);
  const send = () => {
    while (process.send(msg));
    // Wait: backlog of unsent messages exceeds threshold
    setImmediate(send);
  };
  send();
} else {
  const common = require('../common.js');
  const bench = common.createBenchmark(main, {
    len: [
      64, 256, 1024, 4096, 16384, 65536,
      65536 << 4, 65536 << 8,
    ],
    dur: [5]
  });
  const spawn = require('child_process').spawn;

  function main({ dur, len }) {
    bench.start();

    const options = { 'stdio': ['ignore', 1, 2, 'ipc'] };
    const child = spawn(process.argv[0],
                        [process.argv[1], 'child', len], options);

    var bytes = 0;
    child.on('message', (msg) => { bytes += msg.length; });

    setTimeout(() => {
      child.kill();
      bench.end(bytes);
    }, dur * 1000);
  }
}
