'use strict';
if (process.argv[2] === 'child') {
  // Echo every message straight back to the parent.
  process.on('message', (msg) => {
    process.send(msg);
  });
} else {
  const common = require('../common.js');
  const bench = common.createBenchmark(main, {
    len: [64, 256, 1024, 4096, 16384, 65536],
    serialization: ['json', 'advanced'],
    dur: [5],
  });
  const { spawn } = require('child_process');

  function main({ dur, len, serialization }) {
    const msg = { payload: '.'.repeat(len) };
    const options = {
      stdio: ['ignore', 'ignore', 'ignore', 'ipc'],
      serialization,
    };
    const child = spawn(process.argv[0],
                        [process.argv[1], 'child'], options);

    let messages = 0;
    let finished = false;

    child.on('message', () => {
      messages++;
      // Keep one round-trip in flight per completed one so both the serialize
      // (write) and deserialize (read) paths stay saturated on both ends.
      if (!finished)
        child.send(msg);
    });

    bench.start();
    // Prime a window of in-flight messages so the IPC channel never drains.
    for (let i = 0; i < 256; i++)
      child.send(msg);

    setTimeout(() => {
      finished = true;
      bench.end(messages);
      child.kill();
    }, dur * 1000);
  }
}
