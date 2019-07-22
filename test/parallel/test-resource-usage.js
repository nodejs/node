'use strict';
require('../common');
const assert = require('assert');

const rusage = process.resourceUsage();
const fields = [
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
];

assert.deepStrictEqual(Object.keys(rusage).sort(), fields.sort());

fields.forEach((n) => {
  assert.strictEqual(typeof rusage[n], 'number', `${n} should be a number`);
  assert(rusage[n] >= 0, `${n} should be above or equal 0`);
});
