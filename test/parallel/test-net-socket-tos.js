'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(
  common.mustCall((socket) => {
    socket.end();
  }),
);

server.listen(
  0,
  common.mustCall(() => {
    const port = server.address().port;
    const client = new net.Socket();

    // Set TOS before connection to test caching behavior
    client.setTOS(0x10);
    client.connect(port);

    client.on(
      'connect',
      common.mustCall(() => {
        // TEST 1: setTOS validation
        // Should throw if value is not a number, is NaN, or is out of range (0-255)
        assert.throws(() => client.setTOS('invalid'), {
          code: 'ERR_INVALID_ARG_TYPE',
        });
        assert.throws(() => client.setTOS(NaN), {
          code: 'ERR_INVALID_ARG_TYPE',
        });
        assert.throws(() => client.setTOS(256), {
          code: 'ERR_OUT_OF_RANGE',
        });
        assert.throws(() => client.setTOS(-1), {
          code: 'ERR_OUT_OF_RANGE',
        });

        // TEST 2a: Verify deferred application
        // Check if the TOS value set before connect() was cached and applied.
        // We mask with 0xFC to check only the high 6 bits (DSCP),
        // ignoring the lowest 2 bits (ECN) which the OS may modify or zero out.
        const mask = 0xFC;
        const preConnectGot = client.getTOS();
        assert.strictEqual(
          preConnectGot & mask,
          0x10 & mask,
          `Pre-connect TOS should be ${0x10 & mask}, got ${preConnectGot & mask}`,
        );

        // TEST 2b: Setting and getting TOS on an active connection
        const tosValue = 0x10; // IPTOS_LOWDELAY (16)

        // On all platforms, this should succeed (tries both IPv4 and IPv6)
        client.setTOS(tosValue);

        // Verify values
        const got = client.getTOS();

        // Compare only the DSCP bits (7-2) using the mask defined above
        assert.strictEqual(
          got & mask,
          tosValue & mask,
          `Expected TOS ${tosValue & mask}, got ${got & mask}`,
        );

        // TEST 3: Boundary values
        // Check min (0x00), max (0xFF), and arbitrary intermediate values
        for (const boundaryValue of [0x00, 0xFF, 0x3F]) {
          client.setTOS(boundaryValue);
          const gotBoundary = client.getTOS();
          assert.strictEqual(
            gotBoundary & mask,
            boundaryValue & mask,
            `Expected TOS ${boundaryValue & mask}, got ${gotBoundary & mask}`,
          );
        }

        client.end();
      }),
    );

    client.on(
      'end',
      common.mustCall(() => {
        server.close();
      }),
    );
  }),
);
