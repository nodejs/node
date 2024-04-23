'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');

const server = createServer();

server.on('fetch', common.mustCall((event) => {
  event.respondWith(event.request.json().then(({ name }) => {
    return new Response(`Hello, ${name}!`, {
      status: 200,
      headers: {
        'content-type': 'text/plain'
      }
    });
  }));
}));

server.listen(common.mustCall(async () => {
  try {
    const { port } = server.address();

    const req = await fetch(`http://localhost:${port}`, {
      method: 'POST',
      headers: {
        'content-type': 'application/json'
      },
      body: JSON.stringify({
        name: 'world'
      }),
    });

    const body = await req.text();

    assert.strictEqual(body, 'Hello, world!');
  } finally {
    server.close();
  }
}));
