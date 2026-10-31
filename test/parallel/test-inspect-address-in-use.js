'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndExit } = require('../common/child_process');
const { createServer } = require('http');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const entry = fixtures.path('empty.js');
const { Worker } = require('worker_threads');

function testOnServerListen(fn) {
  const server = createServer((socket) => {
    socket.end('echo');
  });

  server.on('listening', () => {
    fn(server);
    server.close();
  });
  server.listen(0, '127.0.0.1');
}

function testChildProcess(getArgs, exitCode, options) {
  testOnServerListen(common.mustCall((server) => {
    const { port } = server.address();
    spawnSyncAndExit(process.execPath, getArgs(port), options, {
      status: exitCode,
      signal: null,
      trim: true,
      stderr: function(str) {
        const match = str.match(
          /Starting inspector on 127\.0\.0\.1:(\d+) failed: address already in use/
        );
        assert.notStrictEqual(match, null);
        assert.strictEqual(match[1], port + '');
      },
    });
  }));
}

tmpdir.refresh();

// V8 does not allow a debugger to be attached during snapshot building.
spawnSyncAndExit(process.execPath, ['--inspect=0', '--build-snapshot', entry],
  { cwd: tmpdir.path },
  {
    status: 9,
    signal: null,
    stderr: /--build-snapshot cannot be used together with --inspect/,
  });

testChildProcess(
  (port) => [`--inspect=${port}`, entry], 0);

testOnServerListen(common.mustCall((server) => {
  const { port } = server.address();
  const worker = new Worker(entry, {
    execArgv: [`--inspect=${port}`]
  });

  worker.on('error', common.mustNotCall());

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}));
