'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSync } = require('child_process');
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
  testOnServerListen((server) => {
    const { port } = server.address();
    const child = spawnSync(process.execPath, getArgs(port), options);
    const stderr = child.stderr.toString().trim();
    const stdout = child.stdout.toString().trim();
    console.log('[STDERR]');
    console.log(stderr);
    console.log('[STDOUT]');
    console.log(stdout);
    const match = stderr.match(
      /Starting inspector on 127\.0\.0\.1:(\d+) failed: address already in use/
    );
    assert.notStrictEqual(match, null);
    assert.strictEqual(match[1], port + '');
    assert.strictEqual(child.status, exitCode);
  });
}

tmpdir.refresh();

testChildProcess(
  (port) => [`--inspect=${port}`, '--build-snapshot', entry], 0,
  { cwd: tmpdir.path });

testChildProcess(
  (port) => [`--inspect=${port}`, entry], 0);

testOnServerListen((server) => {
  const { port } = server.address();
  const worker = new Worker(entry, {
    execArgv: [`--inspect=${port}`]
  });

  worker.on('error', common.mustNotCall());

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
});
