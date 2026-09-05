'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// NodeRequest and NodeResponse are exported.
assert.strictEqual(typeof http.NodeRequest, 'function');
assert.strictEqual(typeof http.NodeResponse, 'function');
assert.ok(http.NodeRequest.prototype instanceof Request);
assert.ok(http.NodeResponse.prototype instanceof Response);

// Publicly constructed instances behave like Request/Response.
{
  const request = new http.NodeRequest('http://example.org/a?b=c', {
    method: 'POST',
    body: 'hello',
    duplex: 'half',
    headers: { 'x-test': '1' },
  });
  assert.ok(request instanceof Request);
  assert.strictEqual(request.method, 'POST');
  assert.strictEqual(request.url, 'http://example.org/a?b=c');
  assert.strictEqual(request.headers.get('x-test'), '1');
  assert.strictEqual(request.remoteAddress, undefined);
  assert.strictEqual(request.encrypted, false);
  request.text().then(common.mustCall((body) => {
    assert.strictEqual(body, 'hello');
  }));
}

// NodeResponse fast path: string body.
(async () => {
  const response = new http.NodeResponse('hello world', { status: 201 });
  assert.ok(response instanceof Response);
  assert.strictEqual(response.status, 201);
  assert.strictEqual(response.ok, true);
  assert.strictEqual(response.statusText, '');
  assert.strictEqual(response.type, 'default');
  assert.strictEqual(response.url, '');
  assert.strictEqual(response.headers.get('content-type'),
                     'text/plain;charset=UTF-8');
  assert.strictEqual(response.bodyUsed, false);
  assert.strictEqual(await response.text(), 'hello world');
  assert.strictEqual(response.bodyUsed, true);
})().then(common.mustCall());

// NodeResponse fast path: Uint8Array body, no default content-type.
(async () => {
  const bytes = new Uint8Array([1, 2, 3]);
  const response = new http.NodeResponse(bytes);
  assert.strictEqual(response.headers.get('content-type'), null);
  assert.deepStrictEqual(new Uint8Array(await response.arrayBuffer()), bytes);
})().then(common.mustCall());

// NodeResponse fast path: headers init and explicit content-type.
{
  const response = new http.NodeResponse('x', {
    headers: { 'content-type': 'text/csv', 'x-a': 'b' },
  });
  assert.strictEqual(response.headers.get('content-type'), 'text/csv');
  assert.strictEqual(response.headers.get('x-a'), 'b');
}

// NodeResponse.json().
(async () => {
  const response = http.NodeResponse.json({ a: 1 });
  assert.strictEqual(response.headers.get('content-type'), 'application/json');
  assert.deepStrictEqual(await response.json(), { a: 1 });
  assert.throws(() => http.NodeResponse.json(undefined), TypeError);
})().then(common.mustCall());

// NodeResponse.clone() before the body is consumed.
(async () => {
  const response = new http.NodeResponse('dup');
  const clone = response.clone();
  assert.strictEqual(await response.text(), 'dup');
  assert.strictEqual(await clone.text(), 'dup');
})().then(common.mustCall());

// NodeResponse validation matches Response.
{
  assert.throws(() => new http.NodeResponse(null, { status: 199 }), RangeError);
  assert.throws(() => new http.NodeResponse(null, { status: 600 }), RangeError);
  assert.throws(() => new http.NodeResponse('x', { status: 204 }), TypeError);
  assert.throws(() => new http.NodeResponse('x', { statusText: 'bad\r\n' }),
                TypeError);
  // Null body statuses without body are fine.
  assert.strictEqual(new http.NodeResponse(null, { status: 204 }).status, 204);
}

// Exotic bodies fall back to the standard Response construction path.
(async () => {
  const params = new URLSearchParams({ a: 'b' });
  const response = new http.NodeResponse(params);
  assert.match(response.headers.get('content-type'),
               /application\/x-www-form-urlencoded/);
  assert.strictEqual(await response.text(), 'a=b');
})().then(common.mustCall());

// Fetch class instances cannot be structured-cloned.
{
  assert.throws(() => structuredClone(new http.NodeResponse('x')),
                { name: 'DataCloneError' });
}

// The handler receives a NodeRequest with metadata getters, an absolute URL,
// combined headers and an immutable Headers object.
{
  const server = http.serve({}, common.mustCall((request) => {
    assert.ok(request instanceof http.NodeRequest);
    assert.ok(request instanceof Request);
    assert.strictEqual(request.method, 'GET');
    assert.strictEqual(request.url, 'http://localhost/test?x=1');
    // URL is parseable and normalized on demand.
    assert.strictEqual(new URL(request.url).searchParams.get('x'), '1');
    // Duplicate headers are combined like the public Headers API does.
    assert.strictEqual(request.headers.get('x-dup'), 'a, b');
    assert.strictEqual(request.headers.get('cookie'), 'k=1; j=2');
    // Incoming request headers are immutable, matching fetch events on
    // other server runtimes.
    assert.throws(() => request.headers.set('x-dup', 'c'), TypeError);
    // Socket metadata is available directly on the request.
    assert.strictEqual(typeof request.remoteAddress, 'string');
    assert.strictEqual(typeof request.remotePort, 'number');
    assert.strictEqual(request.encrypted, false);
    const meta = http.getRemoteMetadata(request);
    assert.strictEqual(meta.remoteAddress, request.remoteAddress);
    return new http.NodeResponse('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('GET /test?x=1 HTTP/1.1\r\nHost: localhost\r\n' +
                   'X-Dup: a\r\nX-Dup: b\r\n' +
                   'Cookie: k=1\r\nCookie: j=2\r\n' +
                   'Connection: close\r\n\r\n');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /content-length: 2/i);
      assert.match(data, /\r\n\r\nok$/);
      server.close();
    }));
  }));
}

// A NodeRequest can be cloned and wrapped in a plain Request.
{
  const server = http.serve({}, common.mustCall(async (request) => {
    const clone = request.clone();
    assert.strictEqual(clone.method, 'POST');
    assert.strictEqual(clone.url, request.url);
    assert.strictEqual(clone.mode, request.mode);
    assert.strictEqual(clone.headers.get('content-type'), 'text/plain');

    // Wrapping transfers the original request's body to the new Request.
    const wrapped = new Request(request, { headers: { 'x-b': '2' } });
    assert.strictEqual(wrapped.url, request.url);
    assert.strictEqual(wrapped.headers.get('x-b'), '2');

    const [first, second] =
      await Promise.all([wrapped.text(), clone.text()]);
    assert.strictEqual(first, 'ping');
    assert.strictEqual(second, 'ping');
    return new http.NodeResponse(first);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\n' +
                   'Content-Type: text/plain\r\nContent-Length: 4\r\n' +
                   'Connection: close\r\n\r\nping');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /\r\n\r\nping$/);
      server.close();
    }));
  }));
}

// Multiple Set-Cookie response headers are written as separate lines.
{
  const server = http.serve({}, common.mustCall((request) => {
    const response = new http.NodeResponse('c');
    response.headers.append('set-cookie', 'a=1');
    response.headers.append('set-cookie', 'b=2; Path=/');
    return response;
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
      assert.match(data, /set-cookie: a=1\r\n/);
      assert.match(data, /set-cookie: b=2; Path=\/\r\n/);
      assert.doesNotMatch(data, /set-cookie: a=1, b=2/);
      server.close();
    }));
  }));
}

// request.signal aborts and the body stream errors when the client
// disconnects before the request body is complete.
{
  let client;
  const server = http.serve({}, common.mustCall((request) => {
    assert.strictEqual(request.signal.aborted, false);
    request.signal.addEventListener('abort', common.mustCall());
    client.destroy();
    return request.text().then(common.mustNotCall(), common.mustCall((err) => {
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(request.signal.aborted, true);
      server.close();
      return new http.NodeResponse(null);
    }));
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    client = net.createConnection(port, common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\n' +
                   'Content-Length: 10\r\n\r\nabc');
    }));
  }));
}

// Response.error() cannot be serialized and turns into a 500.
{
  const server = http.serve({}, common.mustCall((request) => Response.error()));
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

// Echoing the request body through a Response streams it back.
{
  const server = http.serve({}, common.mustCall((request) => {
    return new Response(request.body);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.createConnection(port, common.mustCall(() => {
      client.write('POST / HTTP/1.1\r\nHost: localhost\r\n' +
                   'Content-Length: 4\r\nConnection: close\r\n\r\necho');
    }));

    let data = '';
    client.setEncoding('utf8');
    client.on('data', (chunk) => data += chunk);
    client.on('end', common.mustCall(() => {
      assert.match(data, /HTTP\/1\.1 200/);
      assert.match(data, /echo/);
      server.close();
    }));
  }));
}

// A structurally fetch-compatible response from a different Response class
// (e.g. a framework bundling its own undici) is serialized via its public API.
{
  const server = http.serve({}, common.mustCall((request) => {
    return {
      status: 203,
      statusText: '',
      headers: new Headers({ 'x-foreign': 'yes' }),
      body: null,
      bodyUsed: false,
    };
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
      assert.match(data, /HTTP\/1\.1 203/);
      assert.match(data, /x-foreign: yes/);
      server.close();
    }));
  }));
}
