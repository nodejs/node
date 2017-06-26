'use strict';

const assert = require('assert');
const net = require('net');
const server = net.createServer();

try {
    server.getConnections(() => {
        assert.ok(true, "No error thrown")
    });
}
catch (e) {
    assert.ok(false, "getConnections threw an error");
}
