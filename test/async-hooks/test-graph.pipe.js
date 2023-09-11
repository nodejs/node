'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const spawn = require('child_process').spawn;

const hooks = initHooks();

hooks.enable();
const sleep = spawn('sleep', [ '0.1' ]);

sleep
  .on('exit', common.mustCall(onsleepExit))
  .on('close', common.mustCall(onsleepClose));

function onsleepExit() {}

function onsleepClose() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'PROCESSWRAP', id: 'process:1', triggerAsyncId: null },
      { type: 'PIPEWRAP', id: 'pipe:1', triggerAsyncId: null },
      { type: 'PIPEWRAP', id: 'pipe:2', triggerAsyncId: null },
      { type: 'PIPEWRAP', id: 'pipe:3', triggerAsyncId: null } ],
  );
}
