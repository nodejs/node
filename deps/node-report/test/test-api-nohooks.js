'use strict';

// Testcase to produce report via API call, using the no-hooks/no-signal
// interface - i.e. require('node-report/api')
if (process.argv[2] === 'child') {
  const nodereport = require('../api');
  nodereport.triggerReport();
} else {
  const common = require('./common.js');
  const spawn = require('child_process').spawn;
  const tap = require('tap');

  const child = spawn(process.execPath, [__filename, 'child']);
  child.on('exit', (code) => {
    tap.plan(3);
    tap.equal(code, 0, 'Process exited cleanly');
    const reports = common.findReports(child.pid);
    tap.equal(reports.length, 1, 'Found reports ' + reports);
    const report = reports[0];
    common.validate(tap, report, {pid: child.pid,
      commandline: child.spawnargs.join(' ')
    });
  });
}
