'use strict';

// A reused keep-alive socket must reject unsolicited bytes that arrive after
// the previous response parser is detached but before the next request starts.

const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const http = require('http');
const net = require('net');

if (process.argv[2] === 'server') {
  const server = net.createServer((socket) => {
    let buffered = '';

    socket.on('error', () => socket.destroy());
    socket.on('data', (chunk) => {
      buffered += chunk.toString('latin1');

      while (buffered.includes('\r\n\r\n')) {
        const end = buffered.indexOf('\r\n\r\n') + 4;
        const request = buffered.slice(0, end);
        buffered = buffered.slice(end);

        const requestLine = request.split('\r\n', 1)[0];

        if (requestLine.startsWith('GET /first ')) {
          socket.write(
            'HTTP/1.1 200 OK\r\n' +
            'Content-Length: 3\r\n' +
            'Connection: keep-alive\r\n' +
            '\r\n' +
            'one'
          );

          setTimeout(() => {
            socket.write(
              'HTTP/1.1 200 OK\r\n' +
              'X-Unsolicited: yes\r\n' +
              'Content-Length: 6\r\n' +
              'Connection: keep-alive\r\n' +
              '\r\n' +
              'EXTRA!'
            );
          }, common.platformTimeout(20));
        } else if (requestLine.startsWith('GET /second ')) {
          socket.write(
            'HTTP/1.1 200 OK\r\n' +
            'Content-Length: 3\r\n' +
            'Connection: close\r\n' +
            '\r\n' +
            'two'
          );
        }
      }
    });
  });

  server.listen(0, '127.0.0.1', () => {
    process.send({ event: 'listening', port: server.address().port });
  });
  return;
}

function blockEventLoop(ms) {
  const array = new Int32Array(new SharedArrayBuffer(4));
  Atomics.wait(array, 0, 0, ms);
}

const child = fork(__filename, ['server'], {
  stdio: ['ignore', 'ignore', 'inherit', 'ipc'],
});

child.on('message', common.mustCall((message) => {
  assert.strictEqual(message.event, 'listening');
  runClient(message.port);
}, 1));

function cleanup(agent) {
  agent.destroy();
  child.kill();
}

function runClient(port) {
  const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
  const options = { host: '127.0.0.1', port, agent };

  http.get({ ...options, path: '/first' }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall(() => {
      blockEventLoop(common.platformTimeout(120));
      process.nextTick(() => requestSecond(options, agent));
    }));
  }));
}

function requestSecond(options, agent) {
  const req = http.get({ ...options, path: '/second' }, common.mustCall((res) => {
    let body = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => { body += chunk; });
    res.on('end', common.mustCall(() => {
      assert.strictEqual(res.headers['x-unsolicited'], undefined);
      assert.strictEqual(body, 'two');
      cleanup(agent);
    }));
  }));

  req.on('error', common.mustNotCall());
}
