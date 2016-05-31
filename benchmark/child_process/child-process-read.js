'use strict';
const common = require('../common.js');
const os = require('os');

var messagesLength = [64, 256, 1024, 4096];
// Windows does not support that long arguments
if (os.platform() !== 'win32')
  messagesLength.push(32768);

const bench = common.createBenchmark(main, {
  len: messagesLength,
  dur: [5]
});

const spawn = require('child_process').spawn;
function main(conf) {
  bench.start();

  const dur = +conf.dur;
  const len = +conf.len;

  const msg = '"' + Array(len).join('.') + '"';
  const options = { 'stdio': ['ignore', 'pipe', 'ignore'] };
  const child = spawn('yes', [msg], options);

  var bytes = 0;
  child.stdout.on('data', function(msg) {
    bytes += msg.length;
  });

  setTimeout(function() {
    child.kill();
    const gbits = (bytes * 8) / (1024 * 1024 * 1024);
    bench.end(gbits);
  }, dur * 1000);
}
