'use strict';

require('../common');
const assert = require('assert');
const { createServer } = require('http');

// This test validates that the HTTP server timeouts are properly validated and set.

{
  const server = createServer();
  assert.strictEqual(server.headersTimeout, 60000);
  assert.strictEqual(server.requestTimeout, 300000);
}

{
  const server = createServer({ headersTimeout: 10000, requestTimeout: 20000 });
  assert.strictEqual(server.headersTimeout, 10000);
  assert.strictEqual(server.requestTimeout, 20000);
}

{
  const server = createServer({ headersTimeout: 10000, requestTimeout: 10000 });
  assert.strictEqual(server.headersTimeout, 10000);
  assert.strictEqual(server.requestTimeout, 10000);
}

{
  const server = createServer({ headersTimeout: 10000 });
  assert.strictEqual(server.headersTimeout, 10000);
  assert.strictEqual(server.requestTimeout, 300000);
}

{
  const server = createServer({ requestTimeout: 20000 });
  assert.strictEqual(server.headersTimeout, 20000);
  assert.strictEqual(server.requestTimeout, 20000);
}

{
  const server = createServer({ requestTimeout: 100000 });
  assert.strictEqual(server.headersTimeout, 60000);
  assert.strictEqual(server.requestTimeout, 100000);
}

{
  assert.throws(
    () => createServer({ headersTimeout: 10000, requestTimeout: 1000 }),
    { code: 'ERR_OUT_OF_RANGE' }
  );
}
