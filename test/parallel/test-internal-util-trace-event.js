'use strict';

require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

if (process.env.isChild === '1') {
  const util = require('internal/util');
  if (util.isTraceCategoryEnabled('test')) {
    const traceId = util.getNextTraceEventId();
    util.traceBegin('test', 'demo', traceId);
    util.traceEnd('test', 'demo', traceId);
  }
  return;
}

tmpdir.refresh();
const FILE_NAME = path.join(tmpdir.path, 'node_trace.1.log');

cp.spawnSync(process.execPath,
             [
               '--expose-internals',
               '--trace-events-enabled',
               '--trace-event-categories', 'test',
               __filename,
             ],
             {
               cwd: tmpdir.path,
               env: {
                 ...process.env,
                 isChild: '1',
               },
             });

assert(fs.existsSync(FILE_NAME));
const data = fs.readFileSync(FILE_NAME);
const traces = JSON.parse(data.toString()).traceEvents;
assert(traces.length > 0);
const count = traces.filter((item) => {
  if (item.cat === 'test' && item.name === 'demo') {
    assert.strictEqual(parseInt(item.id) > 0, true);
    return true;
  }
  return false;
}).length;
assert.strictEqual(count === 2, true);
