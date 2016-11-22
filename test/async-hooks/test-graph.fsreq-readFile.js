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
    [ { type: 'FSREQWRAP', id: 'fsreq:1', triggerId: null },
      { type: 'FSREQWRAP', id: 'fsreq:2', triggerId: 'fsreq:1' },
      { type: 'FSREQWRAP', id: 'fsreq:3', triggerId: 'fsreq:2' },
      { type: 'FSREQWRAP', id: 'fsreq:4', triggerId: 'fsreq:3' } ]
  );
}
