// Flags: --expose-internals
'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('Does not support binding fd on Windows');

const assert = require('assert');
const dgram = require('dgram');
const { internalBinding } = require('internal/test/binding');
const { UDP } = internalBinding('udp_wrap');
const { UV_UDP_REUSEADDR } = require('os').constants;

const BUFFER_SIZE = 4096;

// Test binding a fd.
{
  function createHandle(reuseAddr, udp4, bindAddress) {
    let flags = 0;
    if (reuseAddr)
      flags |= UV_UDP_REUSEADDR;

    const handle = new UDP();
    let err = 0;

    if (udp4) {
      err = handle.bind(bindAddress, 0, flags);
    } else {
      err = handle.bind6(bindAddress, 0, flags);
    }
    assert(err >= 0, String(err));
    assert.notStrictEqual(handle.fd, -1);
    return handle;
  }

  function testWithOptions(reuseAddr, udp4) {
    const type = udp4 ? 'udp4' : 'udp6';
    const bindAddress = udp4 ? common.localhostIPv4 : '::1';

    let fd;

    const receiver = dgram.createSocket({
      type,
    });

    receiver.bind({
      port: 0,
      address: bindAddress,
    }, common.mustCall(() => {
      const { port, address } = receiver.address();
      // Create a handle to reuse its fd.
      const handle = createHandle(reuseAddr, udp4, bindAddress);

      fd = handle.fd;
      assert.notStrictEqual(handle.fd, -1);

      const socket = dgram.createSocket({
        type,
        recvBufferSize: BUFFER_SIZE,
        sendBufferSize: BUFFER_SIZE,
      });

      socket.bind({
        port: 0,
        address: bindAddress,
        fd,
      }, common.mustCall(() => {
        // Test address().
        const rinfo = {};
        const err = handle.getsockname(rinfo);
        assert.strictEqual(err, 0);
        const socketRInfo = socket.address();
        assert.strictEqual(rinfo.address, socketRInfo.address);
        assert.strictEqual(rinfo.port, socketRInfo.port);

        // Test buffer size.
        const recvBufferSize = socket.getRecvBufferSize();
        const sendBufferSize = socket.getSendBufferSize();

        // note: linux will double the buffer size
        const expectedBufferSize = common.isLinux ?
          BUFFER_SIZE * 2 : BUFFER_SIZE;
        assert.strictEqual(recvBufferSize, expectedBufferSize);
        assert.strictEqual(sendBufferSize, expectedBufferSize);

        socket.send(String(fd), port, address);
      }));

      socket.on('message', common.mustCall((data) => {
        assert.strictEqual(data.toString('utf8'), String(fd));
        socket.close();
      }));

      socket.on('error', (err) => {
        console.error(err.message);
        assert.fail(err.message);
      });

      socket.on('close', common.mustCall(() => {}));
    }));

    receiver.on('message', common.mustCall((data, { address, port }) => {
      assert.strictEqual(data.toString('utf8'), String(fd));
      receiver.send(String(fd), port, address);
      process.nextTick(() => receiver.close());
    }));

    receiver.on('error', (err) => {
      console.error(err.message);
      assert.fail(err.message);
    });

    receiver.on('close', common.mustCall(() => {}));
  }

  testWithOptions(true, true);
  testWithOptions(false, true);
  if (common.hasIPv6) {
    testWithOptions(false, false);
  }
}
