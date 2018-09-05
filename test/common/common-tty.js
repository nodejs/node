/* eslint-disable node-core/required-modules */
'use strict';

// Named this way for use with the required-modules eslint rule.

// Utilities for TTY related tests.

const assert = require('assert');

// Check TTY status on module load.
checkIsTTY();

function checkIsTTY(filename) {
  let results = '';

  for (const handle of ['stdin', 'stdout', 'stderr']) {
    if (!process[handle].isTTY) {
      results += `> '${handle}' was not a TTY, had an FD of ` +
                 `'${process[handle].fd}',` +
                 ` and was a '${process[handle].constructor.name}'.\n`;
    }
  }

  if (results !== '') {
    console.error('This test must be run connected to a TTY.\n' +
                  results +
                  'Please run this test via the \'tools/test.py\' test runner.');
    process.exit(1);
  }
}

function getTTYfd() {
  // Do our best to grab a tty fd.
  const tty = require('tty');
  const fs = require('fs');
  // Don't attempt fd 0 as it is not writable on Windows.
  // Ref: ef2861961c3d9e9ed6972e1e84d969683b25cf95
  const ttyFd = [1, 2, 4, 5].find(tty.isatty);
  if (ttyFd === undefined) {
    try {
      return fs.openSync('/dev/tty');
    } catch (e) {
      // There aren't any tty fd's available to use.
      return -1;
    }
  }
  return ttyFd;
}

module.exports = {
  getTTYfd
};
