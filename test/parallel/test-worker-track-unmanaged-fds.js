'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');
const { once } = require('events');
const fs = require('fs');

if (!isMainThread)
  common.skip('test needs to be able to freely set `trackUnmanagedFds`');

// All the tests here are run sequentially, to avoid accidentally opening an fd
// which another part of the test expects to be closed.

const preamble = `
const fs = require("fs");
const { parentPort } = require('worker_threads');
const __filename = ${JSON.stringify(__filename)};
process.on('warning', (warning) => parentPort.postMessage({ warning }));
`;

(async () => {
  // Consistency check: Without trackUnmanagedFds, FDs are *not* closed.
  {
    const w = new Worker(`${preamble}
    parentPort.postMessage(fs.openSync(__filename));
    `, { eval: true, trackUnmanagedFds: false });
    const [ [ fd ] ] = await Promise.all([once(w, 'message'), once(w, 'exit')]);
    assert(fd > 2);
    fs.fstatSync(fd); // Does not throw.
    fs.closeSync(fd);
  }

  // With trackUnmanagedFds, FDs are closed automatically.
  {
    const w = new Worker(`${preamble}
    parentPort.postMessage(fs.openSync(__filename));
    `, { eval: true, trackUnmanagedFds: true });
    const [ [ fd ] ] = await Promise.all([once(w, 'message'), once(w, 'exit')]);
    assert(fd > 2);
    assert.throws(() => fs.fstatSync(fd), { code: 'EBADF' });
  }

  // The same, but trackUnmanagedFds is used only as the implied default.
  {
    const w = new Worker(`${preamble}
    parentPort.postMessage(fs.openSync(__filename));
    `, { eval: true });
    const [ [ fd ] ] = await Promise.all([once(w, 'message'), once(w, 'exit')]);
    assert(fd > 2);
    assert.throws(() => fs.fstatSync(fd), { code: 'EBADF' });
  }

  // There is a warning when an fd is unexpectedly opened twice.
  {
    const w = new Worker(`${preamble}
    parentPort.postMessage(fs.openSync(__filename));
    parentPort.once('message', () => {
      const reopened = fs.openSync(__filename);
      fs.closeSync(reopened);
    });
    `, { eval: true, trackUnmanagedFds: true });
    const [ fd ] = await once(w, 'message');
    fs.closeSync(fd);
    w.postMessage('');
    const [ { warning } ] = await once(w, 'message');
    assert.match(warning.message,
                 /File descriptor \d+ opened in unmanaged mode twice/);
  }

  // There is a warning when an fd is unexpectedly closed.
  {
    const w = new Worker(`${preamble}
    parentPort.once('message', (fd) => {
      fs.closeSync(fd);
    });
    `, { eval: true, trackUnmanagedFds: true });
    w.postMessage(fs.openSync(__filename));
    const [ { warning } ] = await once(w, 'message');
    assert.match(warning.message,
                 /File descriptor \d+ closed but not opened in unmanaged mode/);
  }
})().then(common.mustCall());
