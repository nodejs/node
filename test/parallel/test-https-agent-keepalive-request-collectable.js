// Flags: --expose-gc
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A keep-alive socket in the agent's free pool must not keep the request
// that created it reachable.

const assert = require('assert');
const https = require('https');
const fixtures = require('../common/fixtures');
const { gcUntil } = require('../common/gc');

const server = https.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, (req, res) => res.end('ok'));

server.listen(0, common.mustCall(() => {
  const agent = new https.Agent({ keepAlive: true });
  let ref;

  agent.once('free', common.mustCall(() => {
    setImmediate(common.mustCall(async () => {
      assert.strictEqual(Object.keys(agent.freeSockets).length, 1);
      await gcUntil('the first request is collected',
                    () => ref.deref() === undefined);
      agent.destroy();
      server.close();
    }));
  }));

  ref = new WeakRef(https.get({
    port: server.address().port,
    agent,
    rejectUnauthorized: false,
  }, common.mustCall((res) => res.resume())));
}));
