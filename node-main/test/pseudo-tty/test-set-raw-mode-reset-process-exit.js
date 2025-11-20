'use strict';
require('../common');
const child_process = require('child_process');

// Tests that exiting through process.exit() resets the TTY mode.

child_process.spawnSync(process.execPath, [
  '-e', 'process.stdin.setRawMode(true); process.exit(0)',
], { stdio: 'inherit' });

const { stdout } = child_process.spawnSync('stty', {
  stdio: ['inherit', 'pipe', 'inherit'],
  encoding: 'utf8',
});

if (stdout.match(/-echo\b/)) {
  console.log(stdout);
}
