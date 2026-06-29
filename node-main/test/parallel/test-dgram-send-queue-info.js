// Flags: --test-udp-no-try-send
'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const socket = dgram.createSocket('udp4');
assert.strictEqual(socket.getSendQueueSize(), 0);
assert.strictEqual(socket.getSendQueueCount(), 0);
socket.close();

const server = dgram.createSocket('udp4');
const client = dgram.createSocket('udp4');

server.bind(0, common.mustCall(() => {
  client.connect(server.address().port, common.mustCall(() => {
    const data = 'hello';
    client.send(data);
    client.send(data);
    // See uv__send in win/udp.c
    assert.strictEqual(client.getSendQueueSize(),
                       common.isWindows ? 0 : data.length * 2);
    assert.strictEqual(client.getSendQueueCount(), 2);
    client.close();
    server.close();
  }));
}));
