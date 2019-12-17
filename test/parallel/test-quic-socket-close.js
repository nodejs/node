'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createSocket } = require('quic');

const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;

{
  const socket = createSocket({ endpoint: { port: kClientPort } });
  socket.close();
  assert.throws(() => socket.close(), {
    code: 'ERR_QUICSOCKET_DESTROYED',
    name: 'Error',
    message: 'Cannot call close after a QuicSocket has been destroyed'
  });
}

{
  const socket = createSocket({ endpoint: { port: kClientPort } });

  socket.close(common.mustCall(() => {}));
}
