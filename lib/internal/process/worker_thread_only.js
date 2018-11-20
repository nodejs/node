'use strict';

// This file contains process bootstrappers that can only be
// run in the worker thread.

const {
  setupProcessStdio
} = require('internal/process/stdio');

const {
  workerStdio
} = require('internal/worker');

function setupStdio() {
  setupProcessStdio({
    getStdout: () => workerStdio.stdout,
    getStderr: () => workerStdio.stderr,
    getStdin: () => workerStdio.stdin
  });
}

module.exports = {
  setupStdio
};
