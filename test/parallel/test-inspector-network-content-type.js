// Flags: --inspect=0 --experimental-network-inspection
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('node:assert');
const http = require('node:http');
const inspector = require('node:inspector/promises');

const testNetworkInspection = async (session, port, assert) => {
  let assertPromise = assert(session);
  fetch(`http://127.0.0.1:${port}/hello-world`).then(common.mustCall());
  await assertPromise;
  session.removeAllListeners();
  assertPromise = assert(session);
  new Promise((resolve, reject) => {
    const req = http.get(
      {
        host: '127.0.0.1',
        port,
        path: '/hello-world',
      },
      common.mustCall((res) => {
        res.on('data', () => {});
        res.on('end', () => {});
        resolve(res);
      })
    );
    req.on('error', reject);
  });
  await assertPromise;
  session.removeAllListeners();
};

const test = (handleRequest, testSessionFunc) => new Promise((resolve) => {
  const session = new inspector.Session();
  session.connect();
  const httpServer = http.createServer(handleRequest);
  httpServer.listen(0, async () => {
    try {
      await session.post('Network.enable');
      await testNetworkInspection(
        session,
        httpServer.address().port,
        testSessionFunc
      );
      await session.post('Network.disable');
    } catch (err) {
      assert.fail(err);
    } finally {
      await session.disconnect();
      await httpServer.close();
      await inspector.close();
      resolve();
    }
  });
});

(async () => {
  await test(
    (req, res) => {
      res.setHeader('Content-Type', 'text/plain; charset=utf-8');
      res.writeHead(200);
      res.end('hello world\n');
    },
    common.mustCall(
      (session) =>
        new Promise((resolve) => {
          session.on(
            'Network.responseReceived',
            common.mustCall(({ params }) => {
              assert.strictEqual(params.response.mimeType, 'text/plain');
              assert.strictEqual(params.response.charset, 'utf-8');
            })
          );
          session.on(
            'Network.loadingFinished',
            common.mustCall(({ params }) => {
              assert.ok(params.requestId.startsWith('node-network-event-'));
              assert.strictEqual(typeof params.timestamp, 'number');
              resolve();
            })
          );
        }),
      2
    )
  );

  await test(
    (req, res) => {
      res.writeHead(200, {});
      res.end('hello world\n');
    },
    common.mustCall((session) =>
      new Promise((resolve) => {
        session.on(
          'Network.responseReceived',
          common.mustCall(({ params }) => {
            assert.strictEqual(params.response.mimeType, '');
            assert.strictEqual(params.response.charset, '');
          })
        );
        session.on(
          'Network.loadingFinished',
          common.mustCall(({ params }) => {
            assert.ok(params.requestId.startsWith('node-network-event-'));
            assert.strictEqual(typeof params.timestamp, 'number');
            resolve();
          })
        );
      }),
                    2
    )
  );

  await test(
    (req, res) => {
      res.setHeader('Content-Type', 'invalid content-type');
      res.writeHead(200);
      res.end('hello world\n');
    },
    common.mustCall((session) =>
      new Promise((resolve) => {
        session.on(
          'Network.responseReceived',
          common.mustCall(({ params }) => {
            assert.strictEqual(params.response.mimeType, '');
            assert.strictEqual(params.response.charset, '');
          })
        );
        session.on(
          'Network.loadingFinished',
          common.mustCall(({ params }) => {
            assert.ok(params.requestId.startsWith('node-network-event-'));
            assert.strictEqual(typeof params.timestamp, 'number');
            resolve();
          })
        );
      }),
                    2
    )
  );

})().then(common.mustCall());
