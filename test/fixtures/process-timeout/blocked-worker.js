'use strict';

// Keeps the event loop alive with a Worker that is blocked in a synchronous
// native call, so that exiting the process waits for the Worker. The first
// argument is a path at which a file is created once the Worker is blocked.
const path = require('path');
const { Worker } = require('worker_threads');

const script = path.join(__dirname, 'wait-for-parent.js');
const args = [script, String(process.pid), process.argv[2]];
new Worker(`
  require('child_process').execFileSync(process.execPath, ${JSON.stringify(args)}, {
    stdio: 'ignore',
  });
`, { eval: true, name: 'blocked' });
