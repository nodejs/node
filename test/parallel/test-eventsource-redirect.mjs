import '../common/index.mjs';

import assert from 'assert';
import events from 'events';
import http from 'http';

[301, 302, 307, 308].forEach(async (statusCode) => {
  const server = http.createServer((req, res) => {
    if (res.req.url === '/redirect') {
      res.writeHead(statusCode, undefined, { Location: '/target' });
      res.end();
    } else if (res.req.url === '/target') {
      res.writeHead(200, 'dummy', { 'Content-Type': 'text/event-stream' });
      res.end();
    }
  });

  server.listen(0);
  await events.once(server, 'listening');
  const port = server.address().port;

  const eventSourceInstance = new EventSource(`http://localhost:${port}/redirect`);
  eventSourceInstance.onopen = () => {
    assert.strictEqual(eventSourceInstance.url, `http://localhost:${port}/target`);
    eventSourceInstance.close();
    server.close();
  };
});

{
  // Stop trying to connect when getting a 204 response
  const server = http.createServer((req, res) => {
    if (res.req.url === '/redirect') {
      res.writeHead(301, undefined, { Location: '/target' });
      res.end();
    } else if (res.req.url === '/target') {
      res.writeHead(204, 'OK');
      res.end();
    }
  });

  server.listen(0);
  await events.once(server, 'listening');
  const port = server.address().port;

  const eventSourceInstance = new EventSource(`http://localhost:${port}/redirect`);
  eventSourceInstance.onerror = () => {
    assert.strictEqual(eventSourceInstance.url, `http://localhost:${port}/target`);
    assert.strictEqual(eventSourceInstance.readyState, EventSource.CLOSED);
    server.close();
  };
}

{
  // Throw an error when missing a Location header
  const server = http.createServer((req, res) => {
    if (res.req.url === '/redirect') {
      res.writeHead(301, undefined);
      res.end();
    } else if (res.req.url === '/target') {
      res.writeHead(204, 'OK');
      res.end();
    }
  });

  server.listen(0);
  await events.once(server, 'listening');
  const port = server.address().port;

  const eventSourceInstance = new EventSource(`http://localhost:${port}/redirect`);
  eventSourceInstance.onerror = () => {
    assert.strictEqual(eventSourceInstance.url, `http://localhost:${port}/redirect`);
    assert.strictEqual(eventSourceInstance.readyState, EventSource.CLOSED);
    server.close();
  };
}
