'use strict';
require('../common');
const child_process = require('child_process');

// Tests that exiting through normal means resets the TTY mode.
// Refs: https://github.com/nodejs/node/issues/21020

child_process.spawnSync(process.execPath, [
  '-e', 'process.stdin.setRawMode(true)'
], { stdio: 'inherit' });

const { stdout } = child_process.spawnSync('stty', {
  stdio: ['inherit', 'pipe', 'inherit'],
  encoding: 'utf8'
});

if (stdout.match(/-echo\b/)) {
  console.log(stdout);
}
