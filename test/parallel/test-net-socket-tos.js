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
    client.setTOS(0x10);
    client.connect(port);

    client.on(
      'connect',
      common.mustCall(() => {
        // TEST 1: setTOS validation
        // Should throw if value is not a number or out of range
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

        // TEST 2a: verify pre-connect TOS was cached and applied on connect
        // by checking client.getTOS() in the 'connect' event handler
        const mask = 0xFC;
        const preConnectGot = client.getTOS();
        assert.strictEqual(
          preConnectGot & mask,
          0x10 & mask,
          `Pre-connect TOS should be ${0x10 & mask}, got ${preConnectGot & mask}`,
        );

        // TEST 2: setting and getting TOS
        const tosValue = 0x10; // IPTOS_LOWDELAY (16)

        // On all platforms, this should succeed (tries both IPv4 and IPv6)
        client.setTOS(tosValue);

        // Verify values
        // Note: Some OSs might mask the value (e.g. Linux sometimes masks ECN bits),
        // but usually 0x10 should return 0x10.
        const got = client.getTOS();
        // Only compare the upper 6 bits (DSCP, bits 7-2) to avoid ECN/OS-masked bits
        assert.strictEqual(
          got & mask,
          tosValue & mask,
          `Expected TOS ${tosValue & mask}, got ${got & mask}`,
        );

        // Test boundary values
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
