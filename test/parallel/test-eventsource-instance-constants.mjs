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

[
  ['CONNECTING', 0],
  ['OPEN', 1],
  ['CLOSED', 2],
].forEach((config) => {
  {
    const [constant, value] = config;

    const eventSourceInstance = new EventSource(`http://localhost:${port}`);

    // EventSource instance does not expose the constant as an own property.
    assert.strictEqual(Object.hasOwn(eventSourceInstance, constant), false);

    // EventSource instance exposes the constant as an inherited property.
    assert.strictEqual(constant in eventSourceInstance, true);

    // The value is properly set.
    assert.strictEqual(eventSourceInstance[constant], value);

    // The constant is not enumerable.
    assert.strictEqual(Object.prototype.propertyIsEnumerable.call(eventSourceInstance, constant), false);

    // The constant is not writable.
    try {
      eventSourceInstance[constant] = 666;
    } catch (e) {
      assert.strictEqual(e instanceof TypeError, true);
    }
    // The constant is not configurable.
    try {
      delete eventSourceInstance[constant];
    } catch (e) {
      assert.strictEqual(e instanceof TypeError, true);
    }
    assert.strictEqual(eventSourceInstance[constant], value);

    eventSourceInstance.close();
    done++;

    if (done === 3) server.close();
  }
});
