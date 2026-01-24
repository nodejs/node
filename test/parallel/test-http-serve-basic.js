'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Test that serve is exported
assert.strictEqual(typeof http.serve, 'function');
assert.strictEqual(typeof http.getRemoteMetadata, 'function');

// Test basic request/response
{
  const server = http.serve({}, common.mustCall((request) => {
    assert.strictEqual(request.method, 'GET');
    assert.strictEqual(new URL(request.url).pathname, '/test');
    return new Response('Hello World');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET /test HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /Hello World/);
      server.close();
    }));
  }));
}

// Test async handler
{
  const server = http.serve({}, common.mustCall(async (request) => {
    await new Promise((resolve) => setTimeout(resolve, 10));
    return new Response('Async Hello');
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
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /Async Hello/);
      server.close();
    }));
  }));
}

// Test Response.json()
{
  const server = http.serve({}, common.mustCall((request) => {
    return Response.json({ message: 'Hello JSON' });
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
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /application\/json/);
      assert.match(data, /"message":"Hello JSON"/);
      server.close();
    }));
  }));
}

// Test getRemoteMetadata
{
  const server = http.serve({}, common.mustCall((request) => {
    const meta = http.getRemoteMetadata(request);
    assert.strictEqual(typeof meta.remoteAddress, 'string');
    assert.strictEqual(typeof meta.remotePort, 'number');
    assert.strictEqual(typeof meta.localAddress, 'string');
    assert.strictEqual(typeof meta.localPort, 'number');
    assert.strictEqual(meta.encrypted, false);
    return new Response(`Hello from ${meta.remoteAddress}`);
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
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /Hello from/);
      server.close();
    }));
  }));
}

// Test custom status code
{
  const server = http.serve({}, common.mustCall((request) => {
    return new Response('Created', { status: 201, statusText: 'Created' });
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
      assert.match(data, /HTTP\/1\.1 201 Created/);
      server.close();
    }));
  }));
}

// Test custom headers
{
  const server = http.serve({}, common.mustCall((request) => {
    return new Response('With Headers', {
      headers: {
        'X-Custom-Header': 'CustomValue',
        'Content-Type': 'text/plain',
      },
    });
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
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /x-custom-header: CustomValue/i);
      assert.match(data, /content-type: text\/plain/i);
      server.close();
    }));
  }));
}

// Test reading request headers
{
  const server = http.serve({}, common.mustCall((request) => {
    const customHeader = request.headers.get('X-Test-Header');
    return new Response(`Header: ${customHeader}`);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nX-Test-Header: TestValue\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /Header: TestValue/);
      server.close();
    }));
  }));
}

// Test keep-alive connection (multiple requests)
{
  let requestCount = 0;
  const server = http.serve({}, common.mustCall((request) => {
    requestCount++;
    return new Response(`Request ${requestCount}`);
  }, 2));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      // First request
      client.write('GET /1 HTTP/1.1\r\nHost: localhost\r\n\r\n');
    }));

    let data = '';
    let firstResponseReceived = false;
    client.setEncoding('utf8');
    client.on('data', (chunk) => {
      data += chunk;
      if (!firstResponseReceived && data.includes('Request 1')) {
        firstResponseReceived = true;
        // Send second request
        client.write('GET /2 HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
      }
    });
    client.on('end', common.mustCall(() => {
      assert.match(data, /Request 1/);
      assert.match(data, /Request 2/);
      assert.strictEqual(requestCount, 2);
      server.close();
    }));
  }));
}

// Test POST method
{
  const server = http.serve({}, common.mustCall((request) => {
    assert.strictEqual(request.method, 'POST');
    return new Response('POST received');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /POST received/);
      server.close();
    }));
  }));
}

// Test handler as first argument (convenience API)
{
  const server = http.serve(common.mustCall((request) => {
    return new Response('Convenience API');
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
      assert.match(data, /Convenience API/);
      server.close();
    }));
  }));
}

// Test signal for graceful shutdown
{
  const ac = new AbortController();
  const server = http.serve({ signal: ac.signal }, (request) => {
    return new Response('OK');
  });

  server.listen(0, common.mustCall(() => {
    // Abort should close the server
    ac.abort();
  }));

  server.on('close', common.mustCall());
}
