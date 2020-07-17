'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();

server.on('connect', common.mustCall((req, stream) => {
    assert.notStrictEqual(req.url.charAt(0), '/');
    stream.end();
}));

server.listen(0);

server.on('listening', common.mustCall(() => {
    const url = new URL(`http://localhost:${server.address().port}/example.com`);

    const req = http.request(url, {method: 'CONNECT'}).end();
    req.once('connect', common.mustCall(res => {
        res.destroy();
        server.close();
    }))
}));

