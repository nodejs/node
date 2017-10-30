'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// First test. Check that after error event you can listen right away.
{
  const dummyServer = net.Server();
  const server = net.Server();

  // Run some server in order to simulate EADDRINUSE error.
  dummyServer.listen(common.mustCall(() => {
    // Try to listen used port.
    server.listen(dummyServer.address().port);
  }));

  server.on('error', common.mustCall((e) => {
    assert.doesNotThrow(
      () => server.listen(common.mustCall(() => {
        dummyServer.close();
        server.close();
      }))
    );
  }));
}

// Second test. Check that second listen call throws an error.
{
  const server = net.Server();

  server.listen(common.mustCall(() => server.close()));

  common.expectsError(() => server.listen(), {
    code: 'ERR_SERVER_ALREADY_LISTEN',
    type: Error
  });
}

// Third test.
// Check that after the close call you can run listen method just fine.
{
  const server = net.Server();

  server.listen(common.mustCall(() => {
    server.close();
    assert.doesNotThrow(
      () => server.listen(common.mustCall(() => server.close()))
    );
  }));
}
