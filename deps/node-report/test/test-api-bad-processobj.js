'use strict';

// Testcase to check report succeeds if process.versions is damaged
if (process.argv[2] === 'child') {
  // Tamper with the process object
  Object.defineProperty(process, 'versions', {get() { throw 'boom'; }});
  const nodereport = require('../');
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
    const validateOpts = { pid: child.pid, expectedVersions: [],
      commandline: child.spawnargs.join(' '), };
    common.validate(tap, report, validateOpts);
  });
}
