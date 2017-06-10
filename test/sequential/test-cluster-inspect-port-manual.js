'use strict';

/**
 * This test suite checks debug port numbers
 * when node started without --inspect argument
 * and debug initiated with cluster.settings.execArgv or similar
 */

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  let message = 'If master is not debugging, ' +
                'forked worker should not have --inspect argument';

  cluster.fork({message}).on('exit', common.mustCall(checkExitCode));

  message = 'When debugging port is set at cluster.settings, ' +
            'forked worker should have debugPort without offset';
  cluster.settings.execArgv = ['--inspect=' + common.PORT];
  cluster.fork({
    portSet: common.PORT,
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = 'When using custom port number, ' +
            'forked worker should have debugPort incremented from that port';
  cluster.fork({
    portSet: common.PORT + 1,
    message
  }).on('exit', common.mustCall(checkExitCode));

  cluster.fork({
    portSet: common.PORT + 2,
    message
  }).on('exit', common.mustCall(checkExitCode));
} else {
  const hasDebugArg = process.execArgv.some(function(arg) {
    return /--inspect/.test(arg);
  });

  assert.strictEqual(hasDebugArg, process.env.portSet !== undefined);
  hasDebugArg && assert.strictEqual(
    process.debugPort,
    +process.env.portSet,
    process.env.message +
    `; expected port: ${+process.env.portSet}, actual: ${process.debugPort}`
  );
  process.exit();
}

function checkExitCode(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}
