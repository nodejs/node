import {
  mustCall,
  mustNotCall,
} from '../common/index.mjs';

import assert from 'assert';
import events from 'events';
import http from 'http';

{ // Should error if the Content-Type is not text/event-stream
  const server = http.createServer((req, res) => {
    res.writeHead(200, undefined, { 'Content-Type': 'text/plain' });
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
}
