'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
  return;
}

process.env.REPL_TEST_PPID = process.pid;
const child = spawn(process.execPath, [
    '-e',
    'vm.runInThisContext("setInterval(() => console.log(\'miau\'), 1000);", ' +
     '{ breakOnSigint: true });'
]);

child.stderr.once('data', common.mustCall((data) => {
  child.stderr.once('data', common.mustCall((data) => {
    const stdErr = data.toString('utf8');
    assert.ok(
      stdErr.includes('To start debugging, open the following URL in Chrome:'),
      'Expected stdErr to contain "To start debugging,' +
        `open the following URL in Chrome:", got ${stdErr}`
    );

    assert.ok(
      stdErr.includes('chrome-devtools://devtools/bundled'),
      'Expected stdErr to contain "chrome-devtools://devtools/bundled,' +
         `got ${stdErr}`
    );

    process.kill(child.pid, 'SIGINT');
  }));

  const stdErr = data.toString('utf8');
  assert.ok(
    stdErr.includes('Starting inspector agent'),
    `Expected stdErr to contain "Starting inspector agent", got ${stdErr}`
  );
}));

child.stdout.once('data', () => {
  process.kill(child.pid, 'SIGUSR2');
});
