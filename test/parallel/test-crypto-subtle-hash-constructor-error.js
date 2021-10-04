'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto').webcrypto;


async function generateKey() {
    const { privateKey } = await crypto.subtle.generateKey(
		{
			name: 'NODE-ED25519',
			namedCurve: 'NODE-ED25519'
		},
		true,
		['sign', 'verify']
	);
    const signature = await crypto.subtle.sign(
		{
			name: 'NODE-ED25519',
			hash: 'SHA-256'
		},
		privateKey,
		'-'
    );
    return signature;
}

generateKey().catch(common.mustCall((err) => {
  assert.strictEqual(err.name, 'DOMException');
  assert.strictEqual(err.message, 'Hash is not permitted for NODE-ED25519');
  assert.ok(err instanceof DOMException);
}))