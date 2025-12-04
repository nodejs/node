'use strict';
const path = require('path');
const { Worker } = require('worker_threads');
const max_snapshots = parseInt(process.env.TEST_SNAPSHOTS) || 1;
new Worker(path.join(__dirname, 'grow-and-set-near-heap-limit.js'), {
  env: {
    ...process.env,
    limit: max_snapshots,
  },
  resourceLimits: {
    maxOldGenerationSizeMb:
      parseInt(process.env.TEST_OLD_SPACE_SIZE) || 20
  }
});

