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

const xs = 'x'.repeat(1024);
const proc = spawnSync(process.execPath, ['-p', 'process.title', xs]);
strictEqual(proc.stdout.toString().trim(), process.execPath);
