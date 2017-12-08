'use strict';

const common = require('../common');
const net = require('net');

const server = net.createServer().listen(0, connectToServer);

function connectToServer() {
  const client = net.createConnection(this.address().port, () => {
    common.expectsError(() => client.write(1337),
                        {
                          code: 'ERR_INVALID_ARG_TYPE',
                          type: TypeError
                        });

    client.end();
  })
  .on('end', () => server.close());
}
