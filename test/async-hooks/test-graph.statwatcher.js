'use strict';

require('../common');
const commonPath = require.resolve('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const fs = require('fs');

const hooks = initHooks();
hooks.enable();

function onchange() { }
// install first file watcher
fs.watchFile(__filename, onchange);

// install second file watcher
fs.watchFile(commonPath, onchange);

// remove first file watcher
fs.unwatchFile(__filename);

// remove second file watcher
fs.unwatchFile(commonPath);

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'STATWATCHER', id: 'statwatcher:1', triggerAsyncId: null },
      { type: 'STATWATCHER', id: 'statwatcher:2', triggerAsyncId: null } ]
  );
}
