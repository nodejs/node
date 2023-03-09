'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { setDefaultAutoSelectFamilyAttemptTimeout } = require('net');
const { connect } = require('tls');

// Some of the windows machines in the CI need more time to establish connection
setDefaultAutoSelectFamilyAttemptTimeout(common.platformTimeout(common.isWindows ? 1500 : 250));

// Test that TLS connecting works without autoSelectFamily
{
  const socket = connect({
    host: 'nodejs.org',
    port: 443,
    servername: 'nodejs.org',
    autoSelectFamily: false,
  });

  socket.on('secureConnect', common.mustCall(() => socket.end()));
}

// Test that TLS connecting works with autoSelectFamily
{
  const socket = connect({
    host: 'nodejs.org',
    port: 443,
    servername: 'nodejs.org',
    autoSelectFamily: true,
  });

  socket.on('secureConnect', common.mustCall(() => socket.end()));
}
