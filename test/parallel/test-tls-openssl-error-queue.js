// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { fork } = require('node:child_process');
const https = require('node:https');
const fixtures = require('../common/fixtures');

if (process.argv[2] === 'server') {
  const {
    hasOpenSSLErrorForTesting,
    injectOpenSSLErrorForTesting,
  } = require('internal/test/binding').internalBinding('tls_wrap');

  let requests = 0;
  const server = https.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  }, common.mustCall((req, res) => {
    requests++;
    if (requests === 1) {
      res.on('finish', common.mustCall(() => {
        injectOpenSSLErrorForTesting();
        process.send({ injected: true });
      }));
      res.end('first');
      return;
    }

    const queueIsClear = !hasOpenSSLErrorForTesting();
    res.on('finish', common.mustCall(() => {
      server.close(common.mustCall(() => process.disconnect()));
    }));
    res.end(queueIsClear ? 'clear' : 'dirty');
  }, 2));

  server.listen(0, '127.0.0.1', common.mustCall(() => {
    process.send({ port: server.address().port });
  }));
  return;
}

function nextMessage(child) {
  return new Promise((resolve) => child.once('message', resolve));
}

function request(port, agent) {
  return new Promise((resolve, reject) => {
    https.get({
      agent,
      host: '127.0.0.1',
      port,
      rejectUnauthorized: false,
    }, (res) => {
      const socket = res.socket;
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => resolve({ body, socket }));
    }).on('error', reject);
  });
}

async function main() {
  const child = fork(__filename, ['server']);
  const exited = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`server exited with code ${code}, signal ${signal}`));
      }
    });
  });

  const { port } = await nextMessage(child);
  const injected = nextMessage(child);
  const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });
  const first = await request(port, agent);
  assert.strictEqual(first.body, 'first');
  assert.deepStrictEqual(await injected, { injected: true });

  const second = await request(port, agent);
  assert.strictEqual(second.socket, first.socket);
  assert.strictEqual(second.body, 'clear');
  agent.destroy();
  await exited;
}

main().then(common.mustCall());
