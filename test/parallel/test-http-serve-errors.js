'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Test handler throws error (default 500 response)
{
  const server = http.serve({}, common.mustCall((request) => {
    throw new Error('Handler error');
  }));

  server.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'Handler error');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 500/);
      assert.match(data, /Internal Server Error/);
      server.close();
    }));
  }));
}

// Test async handler throws error
{
  const server = http.serve({}, common.mustCall(async (request) => {
    await new Promise((resolve) => setTimeout(resolve, 10));
    throw new Error('Async handler error');
  }));

  server.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'Async handler error');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 500/);
      server.close();
    }));
  }));
}

// Test custom onError handler
{
  const server = http.serve({
    onError: common.mustCall((error, request) => {
      assert.strictEqual(error.message, 'Custom error');
      return new Response('Custom Error Page', { status: 503 });
    }),
  }, common.mustCall((request) => {
    throw new Error('Custom error');
  }));

  server.on('error', common.mustCall());

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 503/);
      assert.match(data, /Custom Error Page/);
      server.close();
    }));
  }));
}

// Test handler returns non-Response
{
  const server = http.serve({}, common.mustCall((request) => {
    return 'not a Response'; // Invalid return value
  }));

  server.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 500/);
      server.close();
    }));
  }));
}

// Test handler returns null
{
  const server = http.serve({}, common.mustCall((request) => {
    return null;
  }));

  server.on('error', common.mustCall());

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 500/);
      server.close();
    }));
  }));
}

// Test getRemoteMetadata with non-serve request
{
  assert.throws(() => {
    const request = new Request('http://example.com/');
    http.getRemoteMetadata(request);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Test invalid arguments to serve()
{
  assert.throws(() => {
    http.serve({}, 'not a function');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// Test onError handler throws
{
  const server = http.serve({
    onError: common.mustCall((error, request) => {
      throw new Error('onError also throws');
    }),
  }, common.mustCall((request) => {
    throw new Error('Handler error');
  }));

  server.on('error', common.mustCall());

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      // Should fallback to default 500 response
      assert.match(data, /HTTP\/1\.1 500/);
      server.close();
    }));
  }));
}

// Test clientError event
{
  const server = http.serve({}, (request) => {
    return new Response('OK');
  });

  server.on('clientError', common.mustCall((err, socket) => {
    assert.ok(err);
    socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      // Send malformed HTTP request
      client.write('INVALID HTTP\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /400/);
      server.close();
    }));
  }));
}

// Test signal already aborted
{
  const ac = new AbortController();
  ac.abort();

  const server = http.serve({ signal: ac.signal }, (request) => {
    return new Response('OK');
  });

  // Set up close handler before listen since server closes immediately
  server.on('close', common.mustCall());

  server.listen(0);
}

// Test async onError handler
{
  const server = http.serve({
    onError: common.mustCall(async (error, request) => {
      await new Promise((resolve) => setTimeout(resolve, 10));
      return new Response('Async Error Handler', { status: 502 });
    }),
  }, common.mustCall((request) => {
    throw new Error('Handler error');
  }));

  server.on('error', common.mustCall());

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 502/);
      assert.match(data, /Async Error Handler/);
      server.close();
    }));
  }));
}
