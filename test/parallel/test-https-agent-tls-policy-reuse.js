'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { once } = require('events');
const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const keys = {
  agent1Cert: fixtures.readKey('agent1-cert.pem'),
  agent1Key: fixtures.readKey('agent1-key.pem'),
  agent3Cert: fixtures.readKey('agent3-cert.pem'),
  agent3Key: fixtures.readKey('agent3-key.pem'),
  agent6Cert: fixtures.readKey('agent6-cert.pem'),
  agent6Key: fixtures.readKey('agent6-key.pem'),
  ca1: fixtures.readKey('ca1-cert.pem'),
  ca2: fixtures.readKey('ca2-cert.pem'),
  ca3: fixtures.readKey('ca3-cert.pem'),
};

function request(server, agent, mode, options = {}) {
  return new Promise((resolve, reject) => {
    const headers = mode === 'session' ? { connection: 'close' } : undefined;
    const req = https.get({
      host: '127.0.0.1',
      port: server.address().port,
      agent,
      headers,
      ...options,
    }, (res) => {
      const isSessionReused = res.socket.isSessionReused();
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => resolve({
        body,
        isSessionReused,
        reusedSocket: req.reusedSocket,
      }));
    });
    req.on('error', reject);
  });
}

async function settleAgent() {
  await new Promise((resolve) => setImmediate(resolve));
}

async function runModes(fn, agentOptions = {}) {
  for (const mode of ['session', 'socket']) {
    const agent = new https.Agent(mode === 'socket' ? {
      keepAlive: true,
      maxCachedSessions: 0,
      ...agentOptions,
    } : agentOptions);
    try {
      await fn(agent, mode);
    } finally {
      agent.destroy();
    }
  }
}

async function withServer(options, handler, fn) {
  const server = https.createServer(options, handler);
  server.on('tlsClientError', () => {});
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  try {
    await fn(server);
  } finally {
    server.close();
    await once(server, 'close');
  }
}

function isCertificateError(error) {
  return error.code === 'SELF_SIGNED_CERT_IN_CHAIN' ||
    error.code === 'UNABLE_TO_VERIFY_LEAF_SIGNATURE' ||
    error.code === 'UNABLE_TO_GET_ISSUER_CERT_LOCALLY' ||
    error.code === 'UNABLE_TO_GET_ISSUER_CERT';
}

async function testEmptyCA() {
  const defaultCAs = tls.getCACertificates('default');
  tls.setDefaultCACertificates([keys.ca1]);
  try {
    await withServer({
      key: keys.agent1Key,
      cert: keys.agent1Cert,
      minVersion: 'TLSv1.2',
      maxVersion: 'TLSv1.2',
    }, (req, res) => res.end('agent1'), common.mustCall(async (server) => {
      await runModes(common.mustCall(async (agent, mode) => {
        const first = await request(server, agent, mode, {
          servername: 'agent1',
        });
        assert.strictEqual(first.body, 'agent1');
        await settleAgent();

        await assert.rejects(request(server, agent, mode, {
          servername: 'agent1',
          ca: [],
        }), isCertificateError);
      }, 2));
    }));
  } finally {
    tls.setDefaultCACertificates(defaultCAs);
  }
}

async function testSecureContextTrust() {
  const trusted = tls.createSecureContext({ ca: keys.ca1 });
  const restricted = tls.createSecureContext({ ca: keys.ca2 });

  await withServer({
    key: keys.agent1Key,
    cert: keys.agent1Cert,
    minVersion: 'TLSv1.2',
    maxVersion: 'TLSv1.2',
  }, (req, res) => res.end('agent1'), common.mustCall(async (server) => {
    await runModes(common.mustCall(async (agent, mode) => {
      const first = await request(server, agent, mode, {
        servername: 'agent1',
        secureContext: trusted,
      });
      assert.strictEqual(first.body, 'agent1');
      await settleAgent();

      await assert.rejects(request(server, agent, mode, {
        servername: 'agent1',
        secureContext: restricted,
      }), isCertificateError);
    }, 2));
  }));
}

async function testSecureContextIdentity() {
  const identity1 = tls.createSecureContext({
    ca: keys.ca1,
    key: keys.agent1Key,
    cert: keys.agent1Cert,
  });
  const identity3 = tls.createSecureContext({
    ca: keys.ca1,
    key: keys.agent3Key,
    cert: keys.agent3Cert,
  });

  await withServer({
    key: keys.agent1Key,
    cert: keys.agent1Cert,
    ca: [keys.ca1, keys.ca2],
    requestCert: true,
    rejectUnauthorized: true,
    minVersion: 'TLSv1.2',
    maxVersion: 'TLSv1.2',
  }, (req, res) => {
    res.end(req.socket.getPeerCertificate().subject.CN);
  }, common.mustCall(async (server) => {
    await runModes(common.mustCall(async (agent, mode) => {
      const first = await request(server, agent, mode, {
        servername: 'agent1',
        secureContext: identity1,
      });
      assert.strictEqual(first.body, 'agent1');
      await settleAgent();

      const second = await request(server, agent, mode, {
        servername: 'agent1',
        secureContext: identity3,
      });
      assert.strictEqual(second.body, 'agent3');
      assert.strictEqual(second.isSessionReused, false);
      assert.strictEqual(second.reusedSocket, false);
    }, 2));
  }));
}

async function testAllowPartialTrustChain() {
  await withServer({
    key: keys.agent6Key,
    cert: keys.agent6Cert,
    minVersion: 'TLSv1.2',
    maxVersion: 'TLSv1.2',
  }, (req, res) => res.end('agent6'), common.mustCall(async (server) => {
    await runModes(common.mustCall(async (agent, mode) => {
      const first = await request(server, agent, mode, {
        servername: 'agent6',
        ca: keys.ca3,
        allowPartialTrustChain: true,
      });
      assert.strictEqual(first.body, 'agent6');
      await settleAgent();

      await assert.rejects(request(server, agent, mode, {
        servername: 'agent6',
        ca: keys.ca3,
      }), isCertificateError);
    }, 2), { checkServerIdentity() {} });
  }));
}

(async function() {
  await testEmptyCA();
  await testSecureContextTrust();
  await testSecureContextIdentity();
  await testAllowPartialTrustChain();
})().then(common.mustCall());
