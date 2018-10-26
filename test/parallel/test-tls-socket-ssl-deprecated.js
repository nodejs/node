// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

const server = tls.createServer({ key, cert }, (socket) => {
  socket.ssl;  // This should trigger a deprecation warning
  socket.setEncoding('utf8');
  socket.pipe(socket);
});
server.listen(0, () => {
  const options = { rejectUnauthorized: false };
  const socket =
    tls.connect(server.address().port, options, common.mustCall(() => {
      socket.end('hello');
    }));
  socket.resume();
  socket.on('end', () => server.close());
});

common.expectWarning('DeprecationWarning',
                     'The tlsSocket.ssl property is deprecated.',
                     'DEP00XX');
