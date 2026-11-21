'use strict';
const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const http = require('http');
const { buffer } = require('stream/consumers');

const firstBody = Buffer.from('regular end of request');
let firstSocket;
let secondBody;

const server = http.createServer(common.mustCall((req, res) => {
  if (firstSocket === undefined) {
    firstSocket = req.socket;
    res.end(firstBody, common.mustCall());
    // The response socket must be uncorked after the first end().
    assert.strictEqual(res.writableCorked, 0);
    res.end(); // Double end the response before reusing the socket.
    return;
  }

  assert.strictEqual(req.socket, firstSocket);
  // Write more than the high water mark so the response has to drain.
  secondBody = Buffer.alloc(req.socket.writableHighWaterMark + 100);
  assert.strictEqual(res.write(secondBody), false);
  res.once('drain', common.mustCall(() => res.end(common.mustCall())));
}, 2));

server.listen(0, common.mustCall(async () => {
  const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
  const options = { port: server.address().port, agent };

  async function getBody() {
    const [res] = await once(http.get(options), 'response');
    return buffer(res);
  }

  assert.deepStrictEqual(await getBody(), firstBody);
  assert.deepStrictEqual(await getBody(), secondBody);
  agent.destroy();
  server.close();
}));
