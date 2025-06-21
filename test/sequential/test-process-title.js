'use strict';
const common = require('../common');
const { spawnSync } = require('child_process');
const { strictEqual } = require('assert');

// FIXME add sunos support
if (common.isSunOS)
  common.skip(`Unsupported platform [${process.platform}]`);
// FIXME add IBMi support
if (common.isIBMi)
  common.skip('Unsupported platform IBMi');

// Explicitly assigning to process.title before starting the child process
// is necessary otherwise *its* process.title is whatever the last
// SetConsoleTitle() call in our process tree set it to.
// Can be removed when https://github.com/libuv/libuv/issues/2667 is fixed.
if (common.isWindows)
  process.title = process.execPath;

const xs = 'x'.repeat(1024);
const proc = spawnSync(process.execPath, ['-p', 'process.title', xs]);
strictEqual(proc.stdout.toString().trim(), process.execPath);
