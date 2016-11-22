'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const exec = require('child_process').exec;

const hooks = initHooks();

hooks.enable();
process.on('SIGUSR2', common.mustCall(onsigusr2, 2));

let count = 0;
exec('kill -USR2 ' + process.pid);

function onsigusr2() {
  count++;

  if (count === 1) {
    // trigger same signal handler again
    exec('kill -USR2 ' + process.pid);
  } else {
    // install another signal handler
    process.removeAllListeners('SIGUSR2');
    process.on('SIGUSR2', common.mustCall(onsigusr2Again));

    exec('kill -USR2 ' + process.pid);
  }
}

function onsigusr2Again() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'SIGNALWRAP', id: 'signal:1', triggerId: null },
      { type: 'PROCESSWRAP', id: 'process:1', triggerId: null },
      { type: 'PIPEWRAP', id: 'pipe:1', triggerId: null },
      { type: 'PIPEWRAP', id: 'pipe:2', triggerId: null },
      { type: 'PIPEWRAP', id: 'pipe:3', triggerId: null },
      { type: 'PROCESSWRAP', id: 'process:2', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:4', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:5', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:6', triggerId: 'signal:1' },
      { type: 'SIGNALWRAP', id: 'signal:2', triggerId: 'signal:1' },
      { type: 'PROCESSWRAP', id: 'process:3', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:7', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:8', triggerId: 'signal:1' },
      { type: 'PIPEWRAP', id: 'pipe:9', triggerId: 'signal:1' } ]
  );
}
