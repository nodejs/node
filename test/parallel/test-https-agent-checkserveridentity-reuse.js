'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');
const { once } = require('events');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');
const expectedError = /rejected by callback/;

function request(options) {
  return new Promise((resolve, reject) => {
    const req = https.get({
      host: '127.0.0.1',
      servername: 'agent1',
      ca: [ca],
      ...options,
    }, (res) => {
      const socket = res.socket;
      res.resume();
      res.on('end', () => resolve({
        socket,
        reusedSocket: req.reusedSocket,
      }));
    });

    req.on('error', reject);
  });
}

const server = https.createServer({
  key,
  cert,
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.2',
}, (req, res) => {
  res.end('ok');
});

(async function() {
  server.listen(0);
  await once(server, 'listening');

  const port = server.address().port;
  let acceptCalls = 0;
  let rejectCalls = 0;
  const acceptingCheck = () => {
    acceptCalls++;
  };
  const rejectingCheck = () => {
    rejectCalls++;
    return new Error('rejected by callback');
  };

  const sessionAgent = new https.Agent();
  const keepAliveAgent = new https.Agent({
    keepAlive: true,
    maxCachedSessions: 0,
  });
  const agentLevelAgent = new https.Agent({
    checkServerIdentity: acceptingCheck,
  });

  try {
    await request({
      port,
      agent: sessionAgent,
      checkServerIdentity: acceptingCheck,
    });
    assert.deepStrictEqual(sessionAgent._sessionCache.map, {});
    await assert.rejects(request({
      port,
      agent: sessionAgent,
      checkServerIdentity: rejectingCheck,
    }), expectedError);

    await request({
      port,
      agent: keepAliveAgent,
      checkServerIdentity: acceptingCheck,
    });
    await assert.rejects(request({
      port,
      agent: keepAliveAgent,
      checkServerIdentity: rejectingCheck,
    }), expectedError);

    const first = await request({
      port,
      agent: agentLevelAgent,
    });
    assert.strictEqual(first.socket.isSessionReused(), false);
    const second = await request({
      port,
      agent: agentLevelAgent,
    });
    assert.strictEqual(second.socket.isSessionReused(), true);

    assert.strictEqual(acceptCalls, 3);
    assert.strictEqual(rejectCalls, 2);
  } finally {
    sessionAgent.destroy();
    keepAliveAgent.destroy();
    agentLevelAgent.destroy();
    server.close();
    await once(server, 'close');
  }
})().then(common.mustCall());
