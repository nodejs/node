/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

if (module.parent) {
  const { spawnSync } = require('child_process');

  function runModuleAs(filename, flags, spawnOptions, role) {
    return spawnSync(process.execPath,
                     [...flags, __filename, role, filename], spawnOptions);
  }

  module.exports = runModuleAs;
  return;
}

const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  if (process.argv[2] === 'worker') {
    new Worker(__filename, {
      workerData: process.argv[3]
    });
    return;
  }
  require(process.argv[3]);
} else {
  require(workerData);
}
