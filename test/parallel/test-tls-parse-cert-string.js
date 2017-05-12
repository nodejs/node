'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

const { tagLFy } = common;

{
  const singles = tagLFy`
    C=US
    ST=CA
    L=SF
    O=Node.js Foundation
    OU=Node.js
    CN=ca1
    emailAddress=ry@clouds.org
  `;
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
  const doubles = tagLFy`
    OU=Domain Control Validated
    OU=PositiveSSL Wildcard
    CN=*.nodejs.org
  `;
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
