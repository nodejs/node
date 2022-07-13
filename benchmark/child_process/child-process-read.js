'use strict';
const common = require('../common.js');

// This benchmark uses `yes` to a create noisy child_processes with varying
// output message lengths, and tries to read 8GB of output

const os = require('os');
const child_process = require('child_process');

const messagesLength = [64, 256, 1024, 4096];
// Windows does not support that long arguments
if (os.platform() !== 'win32')
  messagesLength.push(32768);

const bench = common.createBenchmark(main, {
  len: messagesLength,
  dur: [5]
});

function main({ dur, len }) {
  bench.start();

  const msg = `"${'.'.repeat(len)}"`;
  const options = { 'stdio': ['ignore', 'pipe', 'ignore'] };
  const child = child_process.spawn('yes', [msg], options);

  let bytes = 0;
  child.stdout.on('data', (msg) => {
    bytes += msg.length;
  });

  setTimeout(() => {
    if (process.platform === 'win32') {
      // Sometimes there's a yes.exe process left hanging around on Windows...
      child_process.execSync(`taskkill /f /t /pid ${child.pid}`);
    } else {
      child.kill();
    }
    const gbits = (bytes * 8) / (1024 * 1024 * 1024);
    bench.end(gbits);
  }, dur * 1000);
}
