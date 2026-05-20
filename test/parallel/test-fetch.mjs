import * as common from '../common/index.mjs';

import assert from 'assert';
import events from 'events';
import http from 'http';

assert.strictEqual(typeof globalThis.fetch, 'function');
assert.strictEqual(typeof globalThis.FormData, 'function');
assert.strictEqual(typeof globalThis.Headers, 'function');
assert.strictEqual(typeof globalThis.Request, 'function');
assert.strictEqual(typeof globalThis.Response, 'function');

{
  const asyncFunction = async function() {}.constructor;

  assert.ok(!(fetch instanceof asyncFunction));
  assert.notStrictEqual(Reflect.getPrototypeOf(fetch), Reflect.getPrototypeOf(async function() {}));
  assert.strictEqual(Reflect.getPrototypeOf(fetch), Reflect.getPrototypeOf(function() {}));
}

const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.listen(0);
await events.once(server, 'listening');
const port = server.address().port;

const response = await fetch(`http://localhost:${port}`);

assert(response instanceof Response);
assert.strictEqual(response.status, 200);
assert.strictEqual(response.statusText, 'OK');
const body = await response.text();
assert.strictEqual(body, 'Hello world');

server.close();
