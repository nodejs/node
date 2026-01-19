'use strict';

const { Worker } = require('worker_threads');
const path = require('path');
const max_snapshots = parseInt(process.env.TEST_SNAPSHOTS) || 1;
new Worker(path.join(__dirname, 'grow.js'), {
  execArgv: [
    `--heapsnapshot-near-heap-limit=${max_snapshots}`,
  ],
  resourceLimits: {
    maxOldGenerationSizeMb:
      parseInt(process.env.TEST_OLD_SPACE_SIZE) || 20
  }
});
