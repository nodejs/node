'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer().listen(0, connectToServer);

function connectToServer() {
  const client = net.createConnection(this.address().port, () => {
    assert.throws(() => client.write(1337),
                  common.expectsError({
                    code: 'ERR_INVALID_ARG_TYPE',
                    type: TypeError
                  }));

    client.end();
  })
  .on('end', () => server.close());
}
