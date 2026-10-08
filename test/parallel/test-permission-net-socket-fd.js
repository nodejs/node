'use strict';

// Adopting an existing socket descriptor, as net.Socket({ fd }) and
// server.listen({ fd }) do, requires the net permission. The standard streams
// and the IPC channel of the process can still be adopted without it.

const common = require('../common');
if (common.isWindows) {
  common.skip('Socket descriptors cannot be passed through stdio on Windows');
}

const assert = require('assert');
const { fork, spawn, spawnSync } = require('child_process');
const net = require('net');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'fork-child') {
  assert.strictEqual(process.permission.has('net'), false);
  process.stdout.write('stdout');
  process.once('message', common.mustCall((message) => {
    assert.strictEqual(message, 'ping');
    process.send('pong', () => process.disconnect());
  }));
  process.send('ready');
  return;
}

// The descriptor to adopt is passed as the first argument. Changing
// NODE_CHANNEL_FD at runtime must not make it count as the IPC channel.
const denied = `
  const assert = require('node:assert');
  const net = require('node:net');
  const fd = Number(process.argv[1]);
  process.env.NODE_CHANNEL_FD = String(fd);
  const expected = { code: 'ERR_ACCESS_DENIED', permission: 'Net' };
  assert.throws(() => new net.Socket({ fd }), expected);
  assert.throws(() => net.createServer().listen({ fd }), expected);
  process.send?.('done');
`;

const allowed = `
  const net = require('node:net');
  new net.Socket({ fd: Number(process.argv[1]) }).destroy();
`;

function checkChild(execArgv, source, fd) {
  const { status, signal, stderr } = spawnSync(
    process.execPath,
    [...execArgv, '--eval', source, '3'],
    { stdio: ['ignore', 'ignore', 'pipe', fd] },
  );
  assert.strictEqual(signal, null);
  assert.strictEqual(status, 0, stderr.toString());
}

tmpdir.refresh();

// Connected TCP and Unix domain sockets.
for (const options of [{ host: '127.0.0.1', port: 0 }, { path: common.PIPE }]) {
  let client;
  const server = net.createServer(common.mustCall((socket) => {
    checkChild(['--permission'], denied, socket._handle.fd);
    checkChild(['--permission', '--allow-net'], allowed, socket._handle.fd);
    socket.destroy();
    client.destroy();
    server.close();
  }));
  server.listen(options, common.mustCall(() => {
    const { port } = server.address();
    client = net.connect(options.path ?? { ...options, port });
  }));
}

// A listening TCP socket.
{
  const server = net.createServer(common.mustNotCall());
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    checkChild(['--permission'], denied, server._handle.fd);
    server.close();
  }));
}

// The standard streams are adopted without --allow-net.
{
  const { status, signal, stdout, stderr } = spawnSync(process.execPath, [
    '--permission',
    '--eval',
    'process.stdin.pipe(process.stdout); process.stderr.write("stderr");',
  ], { input: 'stdin' });
  assert.strictEqual(signal, null);
  assert.strictEqual(status, 0, stderr.toString());
  assert.strictEqual(stdout.toString(), 'stdin');
  assert.strictEqual(stderr.toString(), 'stderr');
}

// fork() children get their IPC channel and standard streams without
// --allow-net, including when the IPC channel is not on fd 3.
for (const stdio of [
  ['pipe', 'pipe', 'pipe', 'ipc'],
  ['pipe', 'pipe', 'pipe', 'ignore', 'ipc'],
]) {
  const child = fork(__filename, ['fork-child'], {
    execArgv: ['--permission', '--allow-fs-read=*'],
    stdio,
  });
  let stdout = '';
  let stderr = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => { stdout += chunk; });
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => { stderr += chunk; });
  child.on('message', common.mustCall((message) => {
    if (message === 'ready') {
      child.send('ping');
    } else {
      assert.strictEqual(message, 'pong');
    }
  }, 2));
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(stdout, 'stdout');
  }));
}

// The IPC channel is adopted without --allow-net, but other descriptors of a
// child with an IPC channel still require it.
{
  let client;
  const server = net.createServer(common.mustCall((socket) => {
    const child = spawn(
      process.execPath,
      ['--permission', '--eval', denied, '4'],
      { stdio: ['ignore', 'ignore', 'pipe', 'ipc', socket._handle.fd] },
    );
    let stderr = '';
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.on('message', common.mustCall((message) => {
      assert.strictEqual(message, 'done');
    }));
    child.on('exit', common.mustCall((code, signal) => {
      assert.strictEqual(signal, null);
      assert.strictEqual(code, 0, stderr);
      socket.destroy();
      client.destroy();
      server.close();
    }));
  }));
  server.listen(0, '127.0.0.1', common.mustCall(() => {
    client = net.connect(server.address().port, '127.0.0.1');
  }));
}
