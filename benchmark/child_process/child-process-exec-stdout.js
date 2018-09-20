'use strict';
const common = require('../common.js');
const { exec, execSync } = require('child_process');
const isWindows = process.platform === 'win32';

const messagesLength = [64, 256, 1024, 4096];
// Windows does not support command lines longer than 8191 characters
if (!isWindows) messagesLength.push(32768);

const bench = common.createBenchmark(childProcessExecStdout, {
  len: messagesLength,
  dur: [5]
});

function childProcessExecStdout({ dur, len }) {
  bench.start();

  const maxDuration = dur * 1000;
  const cmd = `yes "${'.'.repeat(len)}"`;
  const child = exec(cmd, { 'stdio': ['ignore', 'pipe', 'ignore'] });

  var bytes = 0;
  child.stdout.on('data', (msg) => {
    bytes += msg.length;
  });

  setTimeout(() => {
    bench.end(bytes);
    if (isWindows) {
      // Sometimes there's a yes.exe process left hanging around on Windows.
      try {
        execSync(`taskkill /f /t /pid ${child.pid}`);
      } catch (_) {
        // this is a best effort kill. stderr is piped to parent for tracing.
      }
    } else {
      child.kill();
    }
  }, maxDuration);
}
