'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const TIMEOUT = 1;

const hooks = initHooks();
hooks.enable();

setTimeout(common.mustCall(ontimeout), TIMEOUT);
function ontimeout() {
  setTimeout(onsecondTimeout, TIMEOUT + 1);
}

function onsecondTimeout() {
  setTimeout(onthirdTimeout, TIMEOUT + 2);
}

function onthirdTimeout() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'Timeout', id: 'timeout:1', triggerAsyncId: null },
      { type: 'Timeout', id: 'timeout:2', triggerAsyncId: 'timeout:1' },
      { type: 'Timeout', id: 'timeout:3', triggerAsyncId: 'timeout:2' }]
  );
}
