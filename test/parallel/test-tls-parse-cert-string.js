'use strict';

// Flags: --expose_internals
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('internal/tls');

{
  const singles = 'C=US\nST=CA\nL=SF\nO=Node.js Foundation\nOU=Node.js\n' +
                  'CN=ca1\nemailAddress=ry@clouds.org';
  const singlesOut = tls.parseCertString(singles);
  assert.deepStrictEqual(singlesOut, {
    C: 'US',
    ST: 'CA',
    L: 'SF',
    O: 'Node.js Foundation',
    OU: 'Node.js',
    CN: 'ca1',
    emailAddress: 'ry@clouds.org'
  });
}

{
  const doubles = 'OU=Domain Control Validated\nOU=PositiveSSL Wildcard\n' +
                  'CN=*.nodejs.org';
  const doublesOut = tls.parseCertString(doubles);
  assert.deepStrictEqual(doublesOut, {
    OU: [ 'Domain Control Validated', 'PositiveSSL Wildcard' ],
    CN: '*.nodejs.org'
  });
}

{
  const invalid = 'fhqwhgads';
  const invalidOut = tls.parseCertString(invalid);
  assert.deepStrictEqual(invalidOut, {});
}

{
  const regexp = new RegExp('^\\(node:\\d+\\) [DEP0073] DeprecationWarning: ' +
                            'tls\\.parseCertString is deprecated$');

  // test for deprecate message
  common.hijackStderr(common.mustCall(function(data) {
    assert.ok(regexp.test(data));
    common.restoreStderr();
  }));

  const ret = require('tls').parseCertString('foo=bar');
  assert.deepStrictEqual(ret, { foo: 'bar' });
}
