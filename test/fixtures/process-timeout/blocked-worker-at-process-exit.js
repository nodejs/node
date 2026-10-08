'use strict';
// Exits the process while a Worker is blocked in a synchronous native call, so
// that exiting the process waits for the Worker. The first argument is a path
// at which a file is created once the process is exiting. The second argument
// is how the process exits: 'exit' calls process.exit(), 'throw' throws an
// uncaught exception, and 'reject' leaves a promise rejection unhandled.
const fs = require('fs');
const path = require('path');
const { Worker } = require('worker_threads');

const [, , marker, mode] = process.argv;
const childMarker = `${marker}.child`;
const script = path.join(__dirname, 'wait-for-parent.js');
const args = [script, String(process.pid), childMarker];
new Worker(`
  require('child_process').execFileSync(process.execPath, ${JSON.stringify(args)}, {
    stdio: 'ignore',
  });
`, { eval: true });

// Once the child process has created its file, the Worker is blocked in
// execFileSync() and can no longer be terminated.
const interval = setInterval(() => {
  if (!fs.existsSync(childMarker)) return;
  clearInterval(interval);
  if (mode === 'throw') throw new Error('boom');
  if (mode === 'reject') return Promise.reject(new Error('boom'));
  process.exit(0);
}, 10);

process.on('exit', () => fs.writeFileSync(marker, ''));
