'use strict';
if (typeof require === 'undefined') {
  console.log('1..0 # Skipped: Not being run as CommonJS');
  process.exit(0);
}

const path = require('path');
const { Worker, SHARE_ENV } = require('worker_threads');

new Worker(path.resolve(process.cwd(), process.argv[2]), { env: SHARE_ENV })
  .on('exit', (code) => process.exitCode = code);
