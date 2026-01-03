// Flags: --no-use-system-ca

// Verify that a worker can enable the system CA store while the parent disables it.

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import https from 'node:https';
import fixtures from '../common/fixtures.js';
import { it, beforeEach, afterEach, describe } from 'node:test';
import { once } from 'events';
import {
  Worker,
  isMainThread,
  parentPort,
  workerData,
} from 'node:worker_threads';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

// To run this test, the system needs to be configured to trust
// the CA certificate first (which needs an interactive GUI approval, e.g. TouchID):
// see the README.md in this folder for instructions on how to do this.
const handleRequest = (req, res) => {
  const path = req.url;
  switch (path) {
    case '/hello-world':
      res.writeHead(200);
      res.end('hello world\n');
      break;
    default:
      common.mustNotCall(`Unexpected path: ${path}`)();
  }
};

describe('use-system-ca', function() {
  async function setupServer(key, cert) {
    const theServer = https.createServer(
      {
        key: fixtures.readKey(key),
        cert: fixtures.readKey(cert),
      },
      handleRequest,
    );
    theServer.listen(0);
    await once(theServer, 'listening');

    return theServer;
  }

  let server;

  beforeEach(async function() {
    if (isMainThread) {
      server = await setupServer('agent8-key.pem', 'agent8-cert.pem');
    } else {
      server = undefined;
    }
  });

  it('trusts a valid root certificate', async function() {
    if (isMainThread) {
      const worker = new Worker(new URL(import.meta.url), {
        execArgv: ['--use-system-ca'],
        workerData: {
          url: `https://localhost:${server.address().port}/hello-world`,
        },
      });
      await assert.rejects(
        fetch(`https://localhost:${server.address().port}/hello-world`),
        (err) => {
          assert.strictEqual(err.cause.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
          return true;
        },
      );

      const [message] = await once(worker, 'message');
      await assert.rejects(
        fetch(`https://localhost:${server.address().port}/hello-world`),
        (err) => {
          assert.strictEqual(err.cause.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
          return true;
        },
      );
      assert.strictEqual(message.ok, true);
      assert.strictEqual(
        message.status,
        200,
        `Expected status 200, got ${message.status}`,
      );
      const [exitCode] = await once(worker, 'exit');
      assert.strictEqual(exitCode, 0);
    } else {
      const { url } = workerData;
      try {
        const res = await fetch(url);
        const text = await res.text();
        parentPort.postMessage({ ok: true, status: res.status, text });
      } catch (err) {
        parentPort.postMessage({
          ok: false,
          code: err?.cause?.code,
          message: err.message,
        });
      }
    }
  });

  afterEach(async function() {
    server?.close();
  });
});
