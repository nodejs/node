'use strict'

const os = require('os');
const path = require('path');
const spawn = require('child_process').spawn;

const gypArgs = ['rebuild'];
if (process.env.NODE_PTY_DEBUG) {
  gypArgs.push('--debug');
}
const gypProcess = spawn(os.platform() === 'win32' ? 'node-gyp.cmd' : 'node-gyp', gypArgs, {
  cwd: path.join(__dirname, '..'),
  stdio: 'inherit'
});

gypProcess.on('exit', function (code) {
  process.exit(code);
});
