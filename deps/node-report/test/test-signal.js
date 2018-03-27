'use strict';

// Testcase to produce report on signal interrupting a js busy-loop,
// showing it is interruptible.
if (process.argv[2] === 'child') {
  require('../');

  // Exit on loss of parent process
  process.on('disconnect', () => process.exit(2));

  function busyLoop() {
    var list = [];
    for (var i = 0; i < 1e10; i++) {
      for (var j = 0; j < 1000; j++) {
        list.push(new MyRecord());
      }
      for (var k = 0; k < 1000; k++) {
        list[k].id += 1;
        list[k].account += 2;
      }
      for (var l = 0; l < 1000; l++) {
        list.pop();
      }
    }
  }

  function MyRecord() {
    this.name = 'foo';
    this.id = 128;
    this.account = 98454324;
  }

  process.send('child started', busyLoop);
} else {
  const common = require('./common.js');
  const fork = require('child_process').fork;
  const tap = require('tap');

  if (common.isWindows()) {
    tap.fail('Unsupported on Windows', { skip: true });
    return;
  }

  const child = fork(__filename, ['child'], { silent: true });
  // Wait for child to indicate it is ready before sending signal
  child.on('message', () => child.kill('SIGUSR2'));
  var stderr = '';
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
    // Terminate the child after the report has been written
    if (stderr.includes('Node.js report completed')) {
      child.kill('SIGTERM');
    }
  });
  child.on('exit', (code, signal) => {
    tap.plan(4);
    tap.equal(code, null, 'Process should not exit cleanly');
    tap.equal(signal, 'SIGTERM', 'Process should exit with expected signal');
    const reports = common.findReports(child.pid);
    tap.equal(reports.length, 1, 'Found reports ' + reports);
    const report = reports[0];
    common.validate(tap, report, {pid: child.pid,
      commandline: child.spawnargs.join(' ')
    });
  });
}
