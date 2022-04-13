'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const fs = require('fs');

const hooks = initHooks();

hooks.enable();
fs.readFile(__filename, common.mustCall(onread));

function onread() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'FSREQCALLBACK', id: 'fsreq:1', triggerAsyncId: null },
      { type: 'FSREQCALLBACK', id: 'fsreq:2', triggerAsyncId: 'fsreq:1' },
      { type: 'FSREQCALLBACK', id: 'fsreq:3', triggerAsyncId: 'fsreq:2' },
      { type: 'FSREQCALLBACK', id: 'fsreq:4', triggerAsyncId: 'fsreq:3' } ]
  );
}
