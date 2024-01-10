import {
  mustCall,
  mustNotCall,
} from '../common/index.mjs';

import assert from 'assert';
import events from 'events';
import http from 'http';

// request-status-error.window.js
// EventSource: incorrect HTTP status code
[204, 205, 210, 299, 404, 410, 503].forEach(async (statusCode) => {
  const server = http.createServer((req, res) => {
    res.writeHead(statusCode, undefined);
    res.end();
  });

  server.listen(0);
  await events.once(server, 'listening');
  const port = server.address().port;

  const eventSourceInstance = new EventSource(`http://localhost:${port}`);
  eventSourceInstance.onopen = mustNotCall();
  eventSourceInstance.onerror = mustCall(function() {
    assert.strictEqual(this.readyState, EventSource.CLOSED);
    server.close();
  });
});
