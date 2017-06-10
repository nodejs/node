'use strict';
// Flags: --inspect={PORT}
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const cluster = require('cluster');
const debuggerPort = common.PORT;

let offset = 0;

function checkExitCode(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

if (cluster.isMaster) {
  assert.strictEqual(process.debugPort, debuggerPort);

  let message = 'If master is debugging with default port, ' +
                'forked worker should have debugPort, with offset = 1';

  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = 'Debug port offset should increment';
  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = 'If master is debugging on non-default port, ' +
            'worker ports should be incremented from it';
  cluster.setupMaster({
    execArgv: ['--inspect=' + common.PORT]
  });
  cluster.fork({
    portSet: common.PORT + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect=hostname:port argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect=localhost:' + (common.PORT + 10)]
  });
  cluster.fork({
    portSet: common.PORT + 10 + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect=ipv4addr:port argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect=' + common.localhostIPv4 + ':' + (common.PORT + 20)]
  });
  cluster.fork({
    portSet: common.PORT + 20 + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect=[ipv6addr]:port argument format ' +
            'should be parsed correctly';
  if (common.hasIPv6) {
    cluster.setupMaster({
      execArgv: ['--inspect=[::1]:' + (common.PORT + 30)]
    });
    cluster.fork({
      portSet: common.PORT + 30 + (++offset),
      message
    }).on('exit', common.mustCall(checkExitCode));
  }

  message = '--inspect=hostname:port argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect=localhost'],
  });
  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect=ipv4addr argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect=' + common.localhostIPv4]
  });
  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect=[ipv6addr] argument format ' +
            'should be parsed correctly';
  if (common.hasIPv6) {
    cluster.setupMaster({
      execArgv: ['--inspect=[::1]']
    });
    cluster.fork({
      portSet: process.debugPort + (++offset),
      message
    }).on('exit', common.mustCall(checkExitCode));
  }

  message = '--inspect --debug-port={PORT} argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect', '--debug-port=' + debuggerPort]
  });
  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

  message = '--inspect --inspect-port={PORT} argument format ' +
            'should be parsed correctly';
  cluster.setupMaster({
    execArgv: ['--inspect', '--inspect-port=' + debuggerPort]
  });
  cluster.fork({
    portSet: process.debugPort + (++offset),
    message
  }).on('exit', common.mustCall(checkExitCode));

} else {
  const hasDebugArg = process.execArgv.some(function(arg) {
    return /inspect/.test(arg);
  });

  assert.strictEqual(hasDebugArg, true);
  assert.strictEqual(
    process.debugPort,
    +process.env.portSet,
    process.env.message
  );
  process.exit();
}
