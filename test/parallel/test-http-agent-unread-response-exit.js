'use strict';

// A complete response that nobody reads must not keep the process alive by
// itself: the socket has nothing left to do, and nothing remains that could
// read the response once the event loop would otherwise be empty.

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const http = require('http');

if (process.argv[2] === 'server') {
  process.on('disconnect', () => process.exit(0));

  const server = http.createServer((req, res) => {
    if (req.url === '/empty') {
      res.writeHead(200);
      res.end();
    } else if (req.url === '/redirect') {
      res.writeHead(302, { Location: '/' });
      res.end('<a href="/">Found</a>');
    } else {
      res.end('hello');
    }
  });

  server.listen(0, () => {
    process.send(server.address().port);
  });
} else {
  const serverProcess = cp.fork(__filename, ['server'], {
    stdio: ['ignore', 'ignore', 'ignore', 'ipc'],
  });
  serverProcess.once('message', common.mustCall((port) => {
    serverProcess.channel.unref();
    serverProcess.unref();
    const agent = new http.Agent({ keepAlive: true });

    // Make requests without reading the responses
    for (const path of ['/empty', '/redirect', '/body']) {
      http.get({ host: common.localhostIPv4, port, agent, path }, common.mustCall());
    }

    // A response that is read later still has its body
    http.get({ host: common.localhostIPv4, port, agent, path: '/body' }, common.mustCall((res) => {
      setTimeout(common.mustCall(() => {
        let body = '';
        res.setEncoding('utf8');
        res.on('data', (chunk) => {
          body += chunk;
        });
        res.on('end', common.mustCall(() => {
          assert.strictEqual(body, 'hello');
        }));
      }), common.platformTimeout(200));
    }));
  }));

  // Unread responses should not block script exit
  setTimeout(common.mustNotCall(), common.platformTimeout(3000)).unref();
}
