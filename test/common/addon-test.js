'use strict';

const common = require('./');
const { spawnSync } = require('child_process');
const { Worker } = require('worker_threads');
const { parseArgs } = require('node:util');

const { values: parsedArgs } = parseArgs({
  options: {
    script: { type: 'string' },
    addon: { type: 'string' },
    role: { type: 'string' },  // 'child', 'worker', or 'worker-thread'
  },
  strict: false,
});

if (require.main === module) {
  if (!parsedArgs.addon || !parsedArgs.role || !parsedArgs.script) {
    throw new Error('addon-test.js requires --addon, --role, and --script parameters');
  }
} else if (!parsedArgs.addon) {
  throw new Error('Tests using addon-test.js require --addon=<name> command line argument');
}

const isInvokedAsChild = !!parsedArgs.role;
const addonPath = `./build/${common.buildType}/${parsedArgs.addon}`;

function spawnTestSync(args = [], options = {}) {
  const { stdio = ['inherit', 'pipe', 'pipe'], env = {}, useWorkerThread = false } = options;
  return spawnSync(process.execPath, [
    ...process.execArgv,
    ...args,
    __filename,
    `--script=${require.main.filename}`,
    `--addon=${parsedArgs.addon}`,
    `--role=${useWorkerThread ? 'worker' : 'child'}`,
  ], {
    ...options,
    env: { ...process.env, ...env },
    stdio,
  });
}

module.exports = {
  addonPath,
  isInvokedAsChild,
  spawnTestSync,
};

if (require.main === module) {
  if (parsedArgs.role === 'worker') {
    new Worker(__filename, {
      argv: [`--script=${parsedArgs.script}`, `--addon=${parsedArgs.addon}`, '--role=worker-thread'],
    });
  } else {
    require(parsedArgs.script);
  }
}
