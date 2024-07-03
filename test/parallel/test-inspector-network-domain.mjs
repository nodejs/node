// Flags: --inspect=0 --experimental-network-inspection

import * as common from '../common/index.mjs';

common.skipIfInspectorDisabled();

import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import http from 'node:http';
import https from 'node:https';
import inspector from 'node:inspector';

const session = new inspector.Session();
session.connect();

const httpServer = http.createServer((req, res) => {
  const path = req.url;
  switch (path) {
    case '/hello-world':
      res.writeHead(200);
      res.end('hello world\n');
      break;
    default:
      assert(false, `Unexpected path: ${path}`);
  }
});

const httpsServer = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, (req, res) => {
  const path = req.url;
  switch (path) {
    case '/hello-world':
      res.writeHead(200);
      res.end('hello world\n');
      break;
    default:
      assert(false, `Unexpected path: ${path}`);
  }
});

const terminate = () => {
  session.disconnect();
  httpServer.close();
  httpsServer.close();
  inspector.close();
};

const testHttpGet = () => new Promise((resolve, reject) => {
  let nodeNetworkEventFinished = false;
  let networkEventFinished = false;

  session.on('NodeNetwork.requestWillBeSent', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(params.request.url, 'http://127.0.0.1/hello-world');
    assert.strictEqual(params.request.method, 'GET');
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.wallTime, 'number');
  }));
  session.on('NodeNetwork.responseReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
  }));
  session.on('NodeNetwork.dataReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.dataLength, 'number');
  }));
  session.on('NodeNetwork.loadingFinished', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    nodeNetworkEventFinished = true;
    if (networkEventFinished) {
      resolve();
    }
  }));

  session.on('Network.requestWillBeSent', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(params.request.url, 'http://127.0.0.1/hello-world');
    assert.strictEqual(params.request.method, 'GET');
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.wallTime, 'number');
  }));
  session.on('Network.responseReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
  }));
  session.on('Network.dataReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.dataLength, 'number');
  }));
  session.on('Network.loadingFinished', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    networkEventFinished = true;
    if (nodeNetworkEventFinished) {
      resolve();
    }
  }));

  http.get({
    host: '127.0.0.1',
    port: httpServer.address().port,
    path: '/hello-world',
  }, common.mustCall());
});

const testHttpsGet = () => new Promise((resolve, reject) => {
  let nodeNetworkEventFinished = false;
  let networkEventFinished = false;

  session.on('NodeNetwork.requestWillBeSent', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(params.request.url, 'https://127.0.0.1/hello-world');
    assert.strictEqual(params.request.method, 'GET');
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.wallTime, 'number');
  }));
  session.on('NodeNetwork.responseReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
  }));
  session.on('NodeNetwork.dataReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.dataLength, 'number');
  }));
  session.on('NodeNetwork.loadingFinished', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    nodeNetworkEventFinished = true;
    if (networkEventFinished) {
      resolve();
    }
  }));

  session.on('Network.requestWillBeSent', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(params.request.url, 'https://127.0.0.1/hello-world');
    assert.strictEqual(params.request.method, 'GET');
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.wallTime, 'number');
  }));
  session.on('Network.responseReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
  }));
  session.on('Network.dataReceived', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    assert.strictEqual(typeof params.dataLength, 'number');
  }));
  session.on('Network.loadingFinished', common.mustCall(({ params }) => {
    assert.ok(params.requestId.startsWith('node-network-event-'));
    assert.strictEqual(typeof params.timestamp, 'number');
    networkEventFinished = true;
    if (nodeNetworkEventFinished) {
      resolve();
    }
  }));

  https.get({
    host: '127.0.0.1',
    port: httpsServer.address().port,
    path: '/hello-world',
    rejectUnauthorized: false,
  }, common.mustCall());
});

const test = async () => {
  await testHttpGet();
  session.removeAllListeners();
  await testHttpsGet();
  session.removeAllListeners();
};

httpServer.listen(0, () => {
  httpsServer.listen(0, () => {
    test().then(common.mustCall()).catch(() => {
      assert.fail();
    }).finally(() => {
      terminate();
    });
  });
});
