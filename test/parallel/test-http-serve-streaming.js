'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Test reading request body
{
  const server = http.serve({}, common.mustCall(async (request) => {
    const body = await request.text();
    return new Response(`Received: ${body}`);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const body = 'Hello Server';
    const client = net.createConnection(port, common.mustCall(() => {
      client.write(`POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n${body}`);
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /Received: Hello Server/);
      server.close();
    }));
  }));
}

// Test reading JSON request body
{
  const server = http.serve({}, common.mustCall(async (request) => {
    const body = await request.json();
    return Response.json({ received: body });
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const body = JSON.stringify({ message: 'Hello' });
    const client = net.createConnection(port, common.mustCall(() => {
      client.write(`POST / HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n${body}`);
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /"received":\{"message":"Hello"\}/);
      server.close();
    }));
  }));
}

// Test streaming response body
{
  const server = http.serve({}, common.mustCall((request) => {
    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(new TextEncoder().encode('chunk1'));
        controller.enqueue(new TextEncoder().encode('chunk2'));
        controller.enqueue(new TextEncoder().encode('chunk3'));
        controller.close();
      },
    });
    return new Response(stream);
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
      assert.match(data, /Transfer-Encoding: chunked/i);
      assert.match(data, /chunk1/);
      assert.match(data, /chunk2/);
      assert.match(data, /chunk3/);
      server.close();
    }));
  }));
}

// Test async streaming response
{
  const server = http.serve({}, common.mustCall((request) => {
    const stream = new ReadableStream({
      async start(controller) {
        await new Promise((resolve) => setTimeout(resolve, 10));
        controller.enqueue(new TextEncoder().encode('async1'));
        await new Promise((resolve) => setTimeout(resolve, 10));
        controller.enqueue(new TextEncoder().encode('async2'));
        controller.close();
      },
    });
    return new Response(stream);
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
      assert.match(data, /async1/);
      assert.match(data, /async2/);
      server.close();
    }));
  }));
}

// Test response with explicit Content-Length (no chunked encoding)
{
  const responseBody = 'Fixed length body';
  const server = http.serve({}, common.mustCall((request) => {
    return new Response(responseBody, {
      headers: {
        'Content-Length': responseBody.length.toString(),
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
      assert.match(data, /Content-Length: 17/i);
      assert.ok(!data.toLowerCase().includes('transfer-encoding'));
      assert.match(data, /Fixed length body/);
      server.close();
    }));
  }));
}

// Test empty response body
{
  const server = http.serve({}, common.mustCall((request) => {
    return new Response(null, { status: 204 });
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
      assert.match(data, /HTTP\/1\.1 204/);
      server.close();
    }));
  }));
}

// Test reading request body as arrayBuffer
{
  const server = http.serve({}, common.mustCall(async (request) => {
    const buffer = await request.arrayBuffer();
    const text = new TextDecoder().decode(buffer);
    return new Response(`ArrayBuffer: ${text}`);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const body = 'binary data';
    const client = net.createConnection(port, common.mustCall(() => {
      client.write(`POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n${body}`);
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /ArrayBuffer: binary data/);
      server.close();
    }));
  }));
}

// Test chunked request body
{
  const server = http.serve({}, common.mustCall(async (request) => {
    const body = await request.text();
    return new Response(`Chunked: ${body}`);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n');
      client.write('5\r\nHello\r\n');
      client.write('6\r\n World\r\n');
      client.write('0\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /Chunked: Hello World/);
      server.close();
    }));
  }));
}

// Test large request body
{
  const largeBody = 'x'.repeat(65536); // 64KB
  const server = http.serve({}, common.mustCall(async (request) => {
    const body = await request.text();
    return new Response(`Length: ${body.length}`);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write(`POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: ${largeBody.length}\r\nConnection: close\r\n\r\n`);
      client.write(largeBody);
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /Length: 65536/);
      server.close();
    }));
  }));
}
