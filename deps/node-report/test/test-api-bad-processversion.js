'use strict';

// Testcase to check report succeeds if process.version is damaged
if (process.argv[2] === 'child') {
  // Tamper with the process object
  delete process['version'];
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
    const validateOpts = { pid: child.pid, expectNodeVersion: true,
      commandline: child.spawnargs.join(' '), };
    common.validate(tap, report, validateOpts);
  });
}
