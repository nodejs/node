// Setting NODE_EXTRA_CA_CERTS to non-existent file emits a warning

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const fork = require('child_process').fork;
const tls = require('tls');

if (process.env.CHILD) {
  // This will try to load the extra CA certs, and emit a warning when it fails.
  return tls.createServer({});
}

const env = Object.assign({}, process.env, {
  CHILD: 'yes',
  NODE_EXTRA_CA_CERTS: `${fixtures.fixturesDir}/no-such-file-exists-🐢`,
});

const opts = {
  env: env,
  silent: true,
};
let stderr = '';

fork(__filename, opts)
  .on('exit', common.mustCall(function(status) {
    assert.strictEqual(status, 0, 'client did not succeed in connecting');
  }))
  .on('close', common.mustCall(function() {
    // TODO(addaleax): Make `SafeGetenv` work like `process.env`
    // encoding-wise
    if (!common.isWindows) {
      const re = /Warning: Ignoring extra certs from.*no-such-file-exists-🐢.* load failed:.*No such file or directory/;
      assert(re.test(stderr), stderr);
    }
  }))
  .stderr.setEncoding('utf8').on('data', function(str) {
    stderr += str;
  });
