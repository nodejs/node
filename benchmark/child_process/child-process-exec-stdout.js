'use strict';
const common = require('../common.js');

var messagesLength = [64, 256, 1024, 4096];
// Windows does not support that long arguments
if (process.platform !== 'win32')
  messagesLength.push(32768);
const bench = common.createBenchmark(main, {
  len: messagesLength,
  dur: [5]
});

const child_process = require('child_process');
const exec = child_process.exec;
function main(conf) {
  bench.start();

  const dur = +conf.dur;
  const len = +conf.len;

  const msg = `"${'.'.repeat(len)}"`;
  msg.match(/./);
  const options = {'stdio': ['ignore', 'pipe', 'ignore']};
  const child = exec(`yes ${msg}`, options);

  var bytes = 0;
  child.stdout.on('data', function(msg) {
    bytes += msg.length;
  });

  setTimeout(function() {
    bench.end(bytes);
    if (process.platform === 'win32') {
      // Sometimes there's a yes.exe process left hanging around on Windows...
      child_process.execSync(`taskkill /f /t /pid ${child.pid}`);
    } else {
      child.kill();
    }
  }, dur * 1000);
}
