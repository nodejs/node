// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();
common.skipIfInspectorDisabled();

const { Session } = require('inspector');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('no crypto');

{
  assert.throws(() => {
    const session = new Session();
    session.connect();
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'Inspector',
  }));
}
