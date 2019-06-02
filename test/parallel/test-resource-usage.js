'use strict';
require('../common');
const assert = require('assert');

const rusage = process.resourceUsage();

[
  'userCPUTime',
  'systemCPUTime',
  'maxRSS',
  'sharedMemorySize',
  'unsharedDataSize',
  'unsharedStackSize',
  'minorPageFault',
  'majorPageFault',
  'swappedOut',
  'fsRead',
  'fsWrite',
  'ipcSent',
  'ipcReceived',
  'signalsCount',
  'voluntaryContextSwitches',
  'involuntaryContextSwitches'
].forEach((n) => {
  assert.strictEqual(typeof rusage[n], 'number', `${n} should be a number`);
  assert(rusage[n] >= 0, `${n} should be above or equal 0`);
});
