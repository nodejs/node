'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  // Ensure reuse of successful sockets.

  const agent = new http.Agent({ keepAlive: true });

  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, common.mustCall(() => {
    let socket;
    http.get({ port: server.address().port, agent })
      .on('response', common.mustCall((res) => {
        socket = res.socket;
        assert(socket);
        res.resume();
        socket.on('free', common.mustCall(() => {
          http.get({ port: server.address().port, agent })
            .on('response', common.mustCall((res) => {
              assert.strictEqual(socket, res.socket);
              assert(socket);
              agent.destroy();
              server.close();
            }));
        }));
      }));
  }));
}

{
  // Ensure that timeouted sockets are not reused.

  const agent = new http.Agent({ keepAlive: true, timeout: 50 });

  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port, agent })
      .on('response', common.mustCall((res) => {
        const socket = res.socket;
        assert(socket);
        res.resume();
        socket.on('free', common.mustCall(() => {
          socket.on('timeout', common.mustCall(() => {
            http.get({ port: server.address().port, agent })
              .on('response', common.mustCall((res) => {
                assert.notStrictEqual(socket, res.socket);
                assert.strictEqual(socket.destroyed, true);
                agent.destroy();
                server.close();
              }));
          }));
        }));
      }));
  }));
}

{
  // Ensure that destroyed sockets are not reused.

  const agent = new http.Agent({ keepAlive: true });

  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(0, common.mustCall(() => {
    let socket;
    http.get({ port: server.address().port, agent })
      .on('response', common.mustCall((res) => {
        socket = res.socket;
        assert(socket);
        res.resume();
        socket.on('free', common.mustCall(() => {
          socket.destroy();
          http.get({ port: server.address().port, agent })
            .on('response', common.mustCall((res) => {
              assert.notStrictEqual(socket, res.socket);
              assert(socket);
              agent.destroy();
              server.close();
            }));
        }));
      }));
  }));
}
