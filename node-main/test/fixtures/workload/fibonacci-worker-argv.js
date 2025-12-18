'use strict';

const { Worker } = require('worker_threads');
const path = require('path');
new Worker(path.join(__dirname, 'fibonacci.js'), {
  execArgv: [
    '--cpu-prof',
    '--cpu-prof-interval',
    process.env.CPU_PROF_INTERVAL || '100'
  ]
});
