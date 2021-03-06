'use strict';
const common = require('../common');
const child_process = require('child_process');

// Tests that exiting through a catchable signal resets the TTY mode.

const proc = child_process.spawn(process.execPath, [
  '-e', 'process.stdin.setRawMode(true); console.log("Y"); while(true) {}'
], { stdio: ['inherit', 'pipe', 'inherit'] });

proc.stdout.on('data', common.mustCall(() => {
  proc.kill('SIGINT');
}));

proc.on('exit', common.mustCall(() => {
  const { stdout } = child_process.spawnSync('stty', {
    stdio: ['inherit', 'pipe', 'inherit'],
    encoding: 'utf8'
  });

  if (stdout.match(/-echo\b/)) {
    console.log(stdout);
  }
}));
