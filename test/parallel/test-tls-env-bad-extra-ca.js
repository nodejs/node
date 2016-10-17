// Setting NODE_EXTRA_CA_CERTS to non-existent file emits a warning

'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');
const fork = require('child_process').fork;

if (process.env.CHILD) {
  // This will try to load the extra CA certs, and emit a warning when it fails.
  return tls.createServer({});
}

const env = {
  CHILD: 'yes',
  NODE_EXTRA_CA_CERTS: common.fixturesDir + '/no-such-file-exists',
};

var opts = {
  env: env,
  silent: true,
};
var stderr = '';

fork(__filename, opts)
  .on('exit', common.mustCall(function(status) {
    assert.equal(status, 0, 'client did not succeed in connecting');
  }))
  .on('close', common.mustCall(function() {
    assert(stderr.match(new RegExp(
      'Warning: Ignoring extra certs from.*no-such-file-exists' +
      '.* load failed:.*No such file or directory'
    )), stderr);
  }))
  .stderr.setEncoding('utf8').on('data', function(str) {
    stderr += str;
  });
