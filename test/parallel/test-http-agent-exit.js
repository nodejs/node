'use strict';

// A response to a HEAD request, or a 204 or 304 response, cannot have a
// message body, so callers have no reason to read it. A keep-alive client
// must still release the socket back to the agent for such responses even if
// the caller never consumes them. Otherwise the socket stays busy and keeps
// the process alive for as long as the server leaves the connection open.
//
// The server runs in a child process and the client process makes unconsumed
// requests and then expects to exit on its own, which it can only do once all
// of its sockets have been freed (and unref'd) by the agent.

const common = require('../common');
const cp = require('child_process');
const http = require('http');

if (process.argv[2] === 'server') {
  const server = http.createServer((req, res) => {
    if (req.method === 'HEAD') {
      res.writeHead(200);
      res.end();
    } else if (req.url === '/204') {
      res.writeHead(204);
      res.end();
    } else if (req.url === '/304') {
      res.writeHead(304);
      res.end();
    }
  });

  server.listen(0, () => {
    process.send(server.address().port);

    // Unref server prior to final mustNotCall, server close will be prevented if sockets are still open
    setTimeout(() => server.unref(), common.platformTimeout(1000));
  });
} else {
  const serverProcess = cp.fork(__filename, ['server'], {
    stdio: ['ignore', 'ignore', 'ignore', 'ipc'],
  });
  serverProcess.once('message', common.mustCall((port) => {
    serverProcess.channel.unref();
    serverProcess.unref();
    const agent = new http.Agent({ keepAlive: true });

    // Make requests without consuming response
    http.get({ method: 'HEAD', host: common.localhostIPv4, port, agent }, common.mustCall());
    http.get({ method: 'GET', host: common.localhostIPv4, port, agent, path: '/204' }, common.mustCall());
    http.get({ method: 'GET', host: common.localhostIPv4, port, agent, path: '/304' }, common.mustCall());

    // Ensure handlers are called/not called as expected
    const cb = common.mustCall((res) => {
      res.on('end', common.mustCall());
      res.on('data', common.mustNotCall());
    }, 3);
    http.get({ method: 'HEAD', host: common.localhostIPv4, port, agent }, cb);
    http.get({ method: 'GET', host: common.localhostIPv4, port, agent, path: '/204' }, cb);
    http.get({ method: 'GET', host: common.localhostIPv4, port, agent, path: '/304' }, cb);
  }));

  // HEAD, 204, and 304 requests should not block script exit
  setTimeout(common.mustNotCall(), common.platformTimeout(3000)).unref();
}
