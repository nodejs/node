'use strict';

const common = require('../common');
const initHooks = require('./init-hooks');
const verifyGraph = require('./verify-graph');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

// A file just over one read chunk (512 KiB): the first request opens, stats
// and hands the fd back, then two chunked reads and a close follow, each
// triggered by the previous request. (Smaller files are a single request.)
tmpdir.refresh();
const file = tmpdir.resolve('graph-readfile.bin');
fs.writeFileSync(file, Buffer.alloc(512 * 1024 + 1, 'x'));

const hooks = initHooks();

hooks.enable();
fs.readFile(file, common.mustCall(onread));

function onread() {}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  verifyGraph(
    hooks,
    [ { type: 'FSREQCALLBACK', id: 'fsreq:1', triggerAsyncId: null },
      { type: 'FSREQCALLBACK', id: 'fsreq:2', triggerAsyncId: 'fsreq:1' },
      { type: 'FSREQCALLBACK', id: 'fsreq:3', triggerAsyncId: 'fsreq:2' },
      { type: 'FSREQCALLBACK', id: 'fsreq:4', triggerAsyncId: 'fsreq:3' } ],
  );
}
