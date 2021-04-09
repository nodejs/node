'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that Node.js doesn't incur a segfault while
// adding session or keylog listeners after destroy.
// https://github.com/nodejs/node/issues/38133
// https://github.com/nodejs/node/issues/38135

const tls = require('tls');
const tlsSocketKeyLog = tls.connect('cause-error');
tlsSocketKeyLog.on('error', () => {
  setTimeout(() => {
    tlsSocketKeyLog.on('keylog', () => { });
  }, 10);
});

const tlsSocketSession = tls.connect('cause-error-2');
tlsSocketSession.on('error', (e) => {
  setTimeout(() => {
    tlsSocketSession.on('session', () => { });
  }, 10);
});
