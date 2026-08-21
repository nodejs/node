'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const fixtures = require('../common/fixtures');

// Test --tls-keylog CLI flag.

const assert = require('assert');
const fs = require('fs');
const { fork } = require('child_process');

if (process.argv[2] === 'test')
  return test();

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const file = tmpdir.resolve('keylog.log');

const child = fork(__filename, ['test'], {
  execArgv: ['--tls-keylog=' + file]
});

child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  const log = fs.readFileSync(file, 'utf8').trim().split('\n');
  // Both client and server should log their secrets,
  // so we should have two identical lines in the log
  assert.strictEqual(log.length, 2);
  assert.strictEqual(log[0], log[1]);
}));

function test() {
  const {
    connect, keys
  } = require(fixtures.path('tls-connect'));

  connect({
    client: {
      checkServerIdentity: (servername, cert) => { },
      ca: `${keys.agent1.cert}\n${keys.agent6.ca}`,
    },
    server: {
      cert: keys.agent6.cert,
      key: keys.agent6.key,
      // Number of keylog events is dependent on protocol version
      maxVersion: 'TLSv1.2',
    },
  }, common.mustCall((err, pair, cleanup) => {
    if (pair.server.err) {
      console.trace('server', pair.server.err);
    }
    if (pair.client.err) {
      console.trace('client', pair.client.err);
    }
    assert.ifError(pair.server.err);
    assert.ifError(pair.client.err);

    return cleanup();
  }));
}
