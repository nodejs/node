'use strict';

// Stays alive until the process whose pid is passed as the first argument has
// exited, so that it does not outlive the test that started it. If a second
// argument is passed, creates a file at that path once it has started.
const fs = require('fs');

const [, , parent, marker] = process.argv;
if (marker) fs.writeFileSync(marker, '');

function parentExited() {
  // On POSIX, the process is reparented as soon as its parent exits, which
  // cannot be confused by the parent's pid being reused.
  if (process.platform !== 'win32') return process.ppid !== Number(parent);
  try {
    process.kill(Number(parent), 0);
    return false;
  } catch {
    return true;
  }
}

setInterval(() => {
  if (parentExited()) process.exit();
}, 50);

// Unblock the parent eventually, even if it is never forced to exit.
setTimeout(() => process.exit(), 60_000);
