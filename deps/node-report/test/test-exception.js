'use strict';

// Testcase to produce report on uncaught exception
if (process.argv[2] === 'child') {
  require('../');

  function myException(request, response) {
    const m = '*** test-exception.js: throwing uncaught Error';
    throw new Error(m);
  }

  myException();

} else {
  const common = require('./common.js');
  const spawn = require('child_process').spawn;
  const tap = require('tap');

  const child = spawn(process.execPath, [__filename, 'child']);
  // Capture stderr output from the child process
  var stderr = '';
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
  });
  child.on('exit', (code) => {
    tap.plan(4);
    tap.equal(code, 1, 'Check for expected process exit code');
    tap.match(stderr, /myException/,
              'Check for expected stack trace frame in stderr');
    const reports = common.findReports(child.pid);
    tap.equal(reports.length, 1, 'Found reports ' + reports);
    const report = reports[0];
    common.validate(tap, report, {pid: child.pid,
      commandline: child.spawnargs.join(' ')
    });
  });
}
