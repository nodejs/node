'use strict';

const common = require('../common');
const net = require('net');

const server = net.createServer().listen(0, connectToServer);

function connectToServer() {
  const client = net.createConnection(this.address().port, () => {
    client.write(1337, common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }));
    client.on('error', common.expectsError({
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }));

    client.destroy();
  })
  .on('close', () => server.close());
}
