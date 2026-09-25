'use strict';

// Cluster workers started with --permission and without --allow-net can use
// their IPC channel and standard streams, which Node.js adopts without the net
// permission.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isPrimary) {
  cluster.setupPrimary({
    execArgv: ['--permission', '--allow-fs-read=*'],
    silent: true,
  });
  const worker = cluster.fork();
  let stdout = '';
  let stderr = '';
  worker.process.stdout.setEncoding('utf8');
  worker.process.stdout.on('data', (chunk) => { stdout += chunk; });
  worker.process.stderr.setEncoding('utf8');
  worker.process.stderr.on('data', (chunk) => { stderr += chunk; });
  worker.on('online', common.mustCall());
  worker.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'ready');
    worker.disconnect();
  }));
  worker.process.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(stdout, 'stdout');
  }));
} else {
  assert.strictEqual(process.permission.has('net'), false);
  process.stdout.write('stdout');
  process.send('ready');
}
