// Flags: --experimental-fetch

'use strict';
const common = require('../common');
const assert = require('assert');
const {fetch, Server} = require('http');

const server = Server(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.write('oh hi');
  res.end();
}));
server.listen(0);

server.on('listening', async () => {
  const port = server.address().port;
  const p = fetch(`http://localhost:${port}`);
  assert(p instanceof fetch.Promise);
  assert(p.then);
  const res = await p;
  const text = await res.text();
  assert.strictEqual(text, 'oh hi');
  server.close();
});
