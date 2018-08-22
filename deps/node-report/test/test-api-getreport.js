'use strict';

// Testcase for returning report as a string via API call

if (process.argv[2] === 'child') {
  const nodereport = require('../');
  console.log(nodereport.getReport());
} else {
  const common = require('./common.js');
  const spawnSync = require('child_process').spawnSync;
  const tap = require('tap');

  const args = [__filename, 'child'];
  const child = spawnSync(process.execPath, args);
  tap.plan(3);
  tap.strictSame(child.stderr.toString(), '',
                 'Checking no messages on stderr');
  const reportFiles = common.findReports(child.pid);
  tap.same(reportFiles, [], 'Checking no report files were written');
  tap.test('Validating report content', (t) => {
    common.validateContent(child.stdout, t, { pid: child.pid,
      commandline: process.execPath + ' ' + args.join(' ')
    });
  });
}
