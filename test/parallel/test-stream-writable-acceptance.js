'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const { stream, Writable } = require('stream');
const { once } = require('events');

async function test() {
    const o = new http.OutgoingMessage();
    assert.strictEqual(o instanceof http.OutgoingMessage, true);
    assert.strictEqual(o instanceof Writable, true);

    let external;

    const server = http.createServer(async (req, nodeStreamResponse) => {
        const isOutgoingMessage = nodeStreamResponse instanceof http.OutgoingMessage;
        //const isStream = stream.isInstance(nodeStreamResponse);
        const isWritable = nodeStreamResponse instanceof Writable;

        assert.strictEqual(nodeStreamResponse.writable, true);
        assert.strictEqual(isOutgoingMessage, true);
        assert.strictEqual(isWritable, true);


        const b = typeof nodeStreamResponse?._writableState

        assert.strictEqual(b, 'object')
        
        const webStreamResponse = Writable.toWeb(nodeStreamResponse);

        setImmediate(common.mustCall(() => {
            external.abort();
            nodeStreamResponse.end('Hello World\n');
        }));
    });

    server.listen(0, common.mustCall(() => {
        external = http.get(`http://127.0.0.1:${server.address().port}`);
        external.on('error', common.mustCall(() => {
            server.close();
          }));
    }));
}

test();