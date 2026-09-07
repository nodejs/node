// Flags: --expose-internals
'use strict';

const common = require('../common');
if (common.isWindows) {
  common.skip('Sending dgram sockets to child processes is not supported');
}

const assert = require('assert');
const dgram = require('dgram');
const { fork, spawn } = require('child_process');

const mode = process.argv[2];
const kStateSymbol = mode === undefined ?
  require('internal/dgram').kStateSymbol : undefined;

if (mode === 'ipc-denied') {
  process.on('message', common.mustNotCall());
  process.send('ready');
} else if (mode === 'ipc-drop') {
  process.once('message', common.mustCall((message, handle) => {
    assert.strictEqual(message, 'socket');
    assert(process.permission.has('net'));

    process.permission.drop('net');
    assert(!process.permission.has('net'));

    handle.once('message', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'after-drop');
      handle.close();
      process.send('received');
    }));
    process.send('receiving');
  }));
  process.send('ready');
} else {
  const deniedSocket = dgram.createSocket('udp4');
  deniedSocket.bind(0, '127.0.0.1', common.mustCall(() => {
    const child = fork(__filename, ['ipc-denied'], {
      execArgv: ['--permission', '--allow-fs-read=*'],
      silent: true,
    });
    let stderr = '';

    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.once('message', common.mustCall((message) => {
      assert.strictEqual(message, 'ready');
      child.send('socket', deniedSocket);
    }));
    child.once('exit', common.mustCall((code, signal) => {
      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
      assert.match(stderr, /ERR_ACCESS_DENIED/);
      assert.match(stderr, /permission: 'Net'/);
      deniedSocket.close();
    }));
  }));

  const droppedSocket = dgram.createSocket('udp4');
  const sender = dgram.createSocket('udp4');
  droppedSocket.bind(0, '127.0.0.1', common.mustCall(() => {
    const { port } = droppedSocket.address();
    const child = fork(__filename, ['ipc-drop'], {
      execArgv: [
        '--permission',
        '--allow-net',
        '--allow-fs-read=*',
      ],
    });
    let timer;

    child.on('message', common.mustCall((message) => {
      if (message === 'ready') {
        child.send('socket', droppedSocket);
      } else if (message === 'receiving') {
        // Sending the socket over IPC keeps this copy open and receiving, so
        // both ends compete for incoming datagrams. Stop receiving before the
        // first one goes out.
        droppedSocket.close();
        timer = setInterval(() => {
          sender.send('after-drop', port, '127.0.0.1');
        }, 10);
      } else {
        assert.strictEqual(message, 'received');
        clearInterval(timer);
        sender.close();
        child.disconnect();
      }
    }, 3));
    child.once('exit', common.mustCall((code, signal) => {
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    }));
  }));

  const fdSocket = dgram.createSocket('udp4');
  fdSocket.bind(0, '127.0.0.1', common.mustCall(() => {
    const source = `
      const assert = require('node:assert');
      const dgram = require('node:dgram');
      const socket = dgram.createSocket('udp4');
      let error;
      try {
        socket.bind({ fd: 3 });
      } catch (err) {
        error = err;
      }
      if (error) {
        assert.strictEqual(error.code, 'ERR_ACCESS_DENIED');
        assert.strictEqual(error.permission, 'Net');
        process.exit(0);
      }
      process.exit(1);
    `;
    const child = spawn(
      process.execPath,
      ['--permission', '--eval', source],
      {
        stdio: [
          'ignore',
          'ignore',
          'pipe',
          fdSocket[kStateSymbol].handle.fd,
        ],
      },
    );
    let stderr = '';

    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (chunk) => { stderr += chunk; });
    child.once('exit', common.mustCall((code, signal) => {
      assert.strictEqual(code, 0, stderr);
      assert.strictEqual(signal, null);
      fdSocket.close();
    }));
  }));
}
