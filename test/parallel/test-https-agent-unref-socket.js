'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');

const request = https.get('https://example.com');

request.on('socket', (socket) => {
  socket.unref();
});
