'use strict';

// Lets the event loop stop while a Worker is blocked in a synchronous native
// call, so that exiting the process waits for the Worker. The first argument is
// a path at which a file is created once the event loop has stopped.
const fs = require('fs');
const path = require('path');
const { Worker } = require('worker_threads');

const marker = process.argv[2];
const childMarker = `${marker}.child`;

const script = path.join(__dirname, 'wait-for-parent.js');
const args = [script, String(process.pid), childMarker];
const worker = new Worker(`
  require('child_process').execFileSync(process.execPath, ${JSON.stringify(args)}, {
    stdio: 'ignore',
  });
`, { eval: true });

// Once the child process has created its file, the Worker is blocked in
// execFileSync() and can no longer be terminated.
const interval = setInterval(() => {
  if (fs.existsSync(childMarker)) {
    clearInterval(interval);
    worker.unref();
  }
}, 10);

process.on('exit', () => fs.writeFileSync(marker, ''));
