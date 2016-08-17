'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
  return;
}

process.env.REPL_TEST_PPID = process.pid;
const child = spawn(process.execPath, [ '-i' ], {
  stdio: [null, null, 2]
});

let stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.pipe(process.stdout);
child.stdout.on('data', function(c) {
  stdout += c;
});

child.stdin.write = ((original) => {
  return (chunk) => {
    process.stderr.write(chunk);
    return original.call(child.stdin, chunk);
  };
})(child.stdin.write);

child.stdout.once('data', common.mustCall(() => {
  process.on('SIGUSR2', common.mustCall(() => {
    process.kill(child.pid, 'SIGINT');
    child.stdout.once('data', common.mustCall(() => {
      // Make sure state from before the interruption is still available.
      child.stdin.end('a*2*3*7\n');
    }));
  }));

  child.stdin.write('a = 1001;' +
                    'process.kill(+process.env.REPL_TEST_PPID, "SIGUSR2");' +
                    'while(true){}\n');
}));

child.on('close', function(code) {
  assert.strictEqual(code, 0);
  assert.notStrictEqual(stdout.indexOf('Script execution interrupted.\n'), -1);
  assert.notStrictEqual(stdout.indexOf('42042\n'), -1);
});
