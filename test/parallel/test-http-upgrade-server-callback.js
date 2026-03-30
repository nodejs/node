'use strict';

const common = require('../common');
const assert = require('assert');

const net = require('net');
const http = require('http');

function testUpgradeCallbackTrue() {
  const server = http.createServer({
    shouldUpgradeCallback: common.mustCall((req) => {
      assert.strictEqual(req.url, '/websocket');
      assert.strictEqual(req.headers.upgrade, 'websocket');

      return true;
    })
  });

  server.on('upgrade', common.mustCall((req, socket, upgradeHead) => {
    assert.strictEqual(req.url, '/websocket');
    assert.strictEqual(req.headers.upgrade, 'websocket');
    assert.ok(socket instanceof net.Socket);
    assert.ok(upgradeHead instanceof Buffer);

    socket.write('HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
      'Upgrade: WebSocket\r\n' +
      'Connection: Upgrade\r\n' +
      '\r\n\r\n');
  }));

  server.on('request', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      path: '/websocket',
      headers: {
        'Upgrade': 'websocket',
        'Connection': 'Upgrade'
      }
    });

    req.on('upgrade', common.mustCall((res, socket, upgradeHead) => {
      assert.strictEqual(res.statusCode, 101);
      assert.ok(socket instanceof net.Socket);
      assert.ok(upgradeHead instanceof Buffer);
      socket.end();
      server.close();

      testUpgradeCallbackFalse();
    }));

    req.on('response', common.mustNotCall());
    req.end();
  }));
}


function testUpgradeCallbackFalse() {
  const server = http.createServer({
    shouldUpgradeCallback: common.mustCall(() => {
      return false;
    })
  });

  server.on('upgrade', common.mustNotCall());

  server.on('request', common.mustCall((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('received but not upgraded');
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      path: '/websocket',
      headers: {
        'Upgrade': 'websocket',
        'Connection': 'Upgrade'
      }
    });

    req.on('upgrade', common.mustNotCall());

    req.on('response', common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', common.mustCall(() => {
        assert.strictEqual(data, 'received but not upgraded');
        server.close();

        testUpgradeCallbackTrueWithoutHandler();
      }));
    }));
    req.end();
  }));
}


function testUpgradeCallbackTrueWithoutHandler() {
  const server = http.createServer({
    shouldUpgradeCallback: common.mustCall(() => {
      return true;
    })
  });

  // N.b: no 'upgrade' handler
  server.on('request', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      path: '/websocket',
      headers: {
        'Upgrade': 'websocket',
        'Connection': 'Upgrade'
      }
    });

    req.on('upgrade', common.mustNotCall());
    req.on('response', common.mustNotCall());

    req.on('error', common.mustCall((e) => {
      assert.strictEqual(e.code, 'ECONNRESET');
      server.close();

      testUpgradeCallbackError();
    }));
    req.end();
  }));
}


function testUpgradeCallbackError() {
  const server = http.createServer({
    shouldUpgradeCallback: common.mustCall(() => {
      throw new Error('should upgrade callback failed');
    })
  });

  server.on('upgrade', common.mustNotCall());
  server.on('request', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const req = http.request({
      port: server.address().port,
      path: '/websocket',
      headers: {
        'Upgrade': 'websocket',
        'Connection': 'Upgrade'
      }
    });

    req.on('upgrade', common.mustNotCall());
    req.on('response', common.mustNotCall());

    process.on('uncaughtException', common.mustCall(() => {
      process.exit(0);
    }));

    req.end();
  }));
}

testUpgradeCallbackTrue();
