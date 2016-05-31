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
