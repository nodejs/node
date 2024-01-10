import '../common/index.mjs';

import assert from 'assert';
import events from 'events';
import http from 'http';

const server = http.createServer((req, res) => {
  res.writeHead(200, 'dummy');
});

server.listen(0);
await events.once(server, 'listening');
const port = server.address().port;

let done = 0;
const eventhandlerIdl = ['onmessage', 'onerror', 'onopen'];

eventhandlerIdl.forEach((type) => {
  const eventSourceInstance = new EventSource(`http://localhost:${port}`);

  // Eventsource eventhandler idl is by default undefined,
  assert.strictEqual(eventSourceInstance[type], undefined);

  // The eventhandler idl is by default not enumerable.
  assert.strictEqual(Object.prototype.propertyIsEnumerable.call(eventSourceInstance, type), false);

  // The eventhandler idl ignores non-functions.
  eventSourceInstance[type] = 7;
  assert.strictEqual(EventSource[type], undefined);

  // The eventhandler idl accepts functions.
  function fn() {}
  eventSourceInstance[type] = fn;
  assert.strictEqual(eventSourceInstance[type], fn);

  eventSourceInstance.close();
  done++;

  if (done === eventhandlerIdl.length) server.close();
});
