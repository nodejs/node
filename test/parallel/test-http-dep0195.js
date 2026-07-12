'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// DEP0195: Instantiating node:http classes without `new` is runtime-deprecated.
// Deprecation codes warn once per process, so only the first call emits a warning.

common.expectWarning(
  'DeprecationWarning',
  "Instantiating Agent without the 'new' keyword has been deprecated.",
  'DEP0195',
);

{
  const agent = http.Agent();
  assert.ok(agent instanceof http.Agent);
  agent.destroy();
}

// Remaining classes still construct without `new` (same deprecation code; no
// additional warnings are emitted).
{
  const server = http.Server();
  assert.ok(server instanceof http.Server);
  server.close();
}

{
  const msg = http.OutgoingMessage();
  assert.ok(msg instanceof http.OutgoingMessage);
}

{
  const msg = http.IncomingMessage();
  assert.ok(msg instanceof http.IncomingMessage);
}

{
  const req = new http.IncomingMessage();
  const res = http.ServerResponse(req);
  assert.ok(res instanceof http.ServerResponse);
}

{
  // ClientRequest begins a real request; provide a fake socket so nothing is
  // dialed on the network.
  const { Socket } = require('net');
  const agent = new http.Agent();
  agent.createConnection = common.mustCall((options, cb) => {
    const socket = new Socket();
    // Never connect; destroy immediately after construction completes.
    process.nextTick(() => {
      socket.destroy();
      if (typeof cb === 'function') cb(new Error('stop'));
    });
    return socket;
  });
  const req = http.ClientRequest({
    host: '127.0.0.1',
    port: 1,
    agent,
  });
  assert.ok(req instanceof http.ClientRequest);
  req.on('error', common.mustCall());
  req.destroy();
}
