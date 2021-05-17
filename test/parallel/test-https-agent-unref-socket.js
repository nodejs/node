'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');

const request = https.get('https://www.google.com');

request.on('socket', (socket) => {
  socket.unref();
});
