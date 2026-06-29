'use strict';

const common = require('../common');
const { addresses: { INET_HOST } } = require('../common/internet');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { connect } = require('tls');

// Test that TLS connecting works without autoSelectFamily
{
  const socket = connect({
    host: INET_HOST,
    port: 443,
    servername: INET_HOST,
    autoSelectFamily: false,
  });

  socket.on('secureConnect', common.mustCall(() => socket.end()));
}

// Test that TLS connecting works with autoSelectFamily
{
  const socket = connect({
    host: INET_HOST,
    port: 443,
    servername: INET_HOST,
    autoSelectFamily: true,
  });

  socket.on('secureConnect', common.mustCall(() => socket.end()));
}
