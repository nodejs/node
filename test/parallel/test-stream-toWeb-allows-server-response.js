'use strict';
const common = require('../common');
const { Writable } = require('stream');

// test the change i made
const assert = require('assert');
const http = require('http');

// check if writeable.toWeb successfully works on the response object after creating a server
const server = http.createServer((req, res) => {
    const webStreamResponse = Writable.toWeb(res);
    // check instance of webStreamResponse
    assert.strictEqual(webStreamResponse instanceof Writable, true);
});

server.listen(0, common.mustCall(() => {
    server.close();
}));


